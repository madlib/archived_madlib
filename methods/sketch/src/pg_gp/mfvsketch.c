/*!
 * \file mfvsketch.c
 *
 * \brief CountMin sketch for Most Frequent Value estimation
 */

/*!
 * \defgroup mfvsketch MFV (Most Frequent Values)
 * \ingroup sketches
 * \par About
 * MFVSketch: Most Frequent Values variant of CountMin sketch, implemented
 * as a UDA.
 *
 * \implementation
 * This is basically a CountMin sketch that keeps track of most frequent values
 * as it goes.  This is easy to do, because at any point during a scan,
 * it can use the CM sketch to quickly get the count of any value so far.
 *
 * \usage
 * The MFV frequent-value UDA comes in two different versions: a "quick and dirty" version that does
 * parallel aggregation, and a more faithful implementation that preserves the
 * approximation guarantees of Cormode/Muthukrishnan, but ships all rows to the
 * master for aggregation.
 *
 * It only uses CountMin sketches for value counting, and doesn't need the "dyadic" range trick.
 * As a result it's not limited to integers, and the implementation works
 * for any Postgres data type.
 *
 * The standard method here (<c>mfvsketch_top_histogram</c>) provides epsilon/delta
 * probabilistic guarantees of accuracy, but does not do any parallel aggregation.
 * The parallel method (<c>mfvsketch_quick_histogram</c>) is a heuristic with no
 * such guarantees, but it will likely work well in most cases.  As an example
 * of a case where it will fail, consider a scenario where the top <i>n</i> values on node 1 are very infrequent on
 * node 2, and the top <i>n</i> values on node 2 are infrequent on node 1.  But the <i>n</i>+1'th value
 * is the same on both nodes and the most frequent value in toto.  It will get
 * surpressed incorrectly by the parallel heuristic, but get chosen by the standard method.
 *
 * However, we're probably OK here most of the time.  What we're interested in are
 * values whose frequencies are unusually high.  For
 * columns with very flat distributions, we likely don't care about the results much.
 * Otherwise, The results of this heuristic will likely be
 * unusually frequent values, if not precisely the *most* frequent values.
 *
 *  - <c>mfvsketch_top_histogram(col anytype, nbuckets int4)</c>
 *   is a UDA over column <c>col</c> of any type, and a number of buckets <c>nbuckets</c>,
 *   and produces an n-bucket histogram for the column where each bucket counts one of the
 *   most frequent values in the column. The output is an array of doubles {value, count}
 *   in descending order of frequency; counts are approximate. Ties are handled arbitrarily.
 *   Example:\code
 *   SELECT madlib.mfvsketch_top_histogram(proname, 4)
 *     FROM pg_proc;
 *   \endcode
 *
 *  - <c>mfvsketch_quick_histogram(col anytype, nbuckets int4)</c> is the same as the above
 *  but, in Greenplum it does parallel aggregation to provide a "quick and dirty"
 *  answer.  In Postgres it is identical to <c>mfvsketch_top_histogram</c>. Example:\code
 *   SELECT madlib.mfvsketch_quick_histogram(proname, 4)
 *     FROM pg_proc;
 * \endcode
 *
 * \literature
 * This method is not usually called an MFV sketch in the literature, it
 * is simply an application of the CountMin sketch.  We make the
 * distinction here because of implementation details: we use CountMin in
 * a stylized way (a single dyadic range, postgres anyelement type), and we
 * need to maintain the running histogram of top values.
 * \sa countmin
 */

#include "postgres.h"
#include "utils/array.h"
#include "utils/elog.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/numeric.h"
#include "utils/typcache.h"
#include "nodes/execnodes.h"
#include "fmgr.h"
#include "sketch_support.h"
#include "catalog/pg_type.h"
#include "countmin.h"
#include "funcapi.h"

#include <ctype.h>

PG_FUNCTION_INFO_V1(__mfvsketch_trans);

/*!
 *  transition function to maintain a CountMin sketch with
 *  Most-Frequent Values
 */
Datum __mfvsketch_trans(PG_FUNCTION_ARGS)
{
    bytea *      transblob = PG_GETARG_BYTEA_P(0);
    Datum        newdatum  = PG_GETARG_DATUM(1);
    int          max_mfvs  = PG_GETARG_INT32(2);
    mfvtransval *transval;
    uint64       tmpcnt;
    int          i;
    char *       outString;

    /*
     * This function makes destructive updates to its arguments.
     * Make sure it's being called in an agg context.
     */
    if (!(fcinfo->context &&
          (IsA(fcinfo->context, AggState)
   #ifdef NOTGP
           || IsA(fcinfo->context, WindowAggState)
   #endif
          )))
        elog(ERROR,
             "destructive pass by reference outside agg");

    /* initialize if this is first call */
    if (VARSIZE(transblob) <= sizeof(MFV_TRANSVAL_SZ(0))) {
        Oid typOid = get_fn_expr_argtype(fcinfo->flinfo, 1);
        transblob = mfv_init_transval(max_mfvs, typOid);
    }

    /* ignore NULL inputs */
    if (PG_ARGISNULL(1) || PG_ARGISNULL(2))
        PG_RETURN_DATUM(PointerGetDatum(transblob));

    transval = (mfvtransval *)VARDATA(transblob);
    /* insert into the countmin sketch */
    outString = countmin_trans_c(transval->sketch,
                                 newdatum,
                                 transval->outFuncOid);

    tmpcnt = cmsketch_count_c(transval->sketch,
                              newdatum,
                              transval->outFuncOid);
    i = mfv_find(transblob, newdatum);

    if (i > -1) {
        transval->mfvs[i].cnt = tmpcnt;
    }
    else {
        /* try to insert as either a new or replacement entry */
        for (i = 0; i < (int)transval->max_mfvs; i++) {
            if ((i == (int)transval->next_mfv)) {
                /* room for new */
                transblob = mfv_transval_append(transblob, newdatum);
                transval = (mfvtransval *)VARDATA(transblob);
                transval->mfvs[i].cnt = tmpcnt;
                break;
            }
            else if (transval->mfvs[i].cnt < tmpcnt) {
                /* arg beats this mfv */
                transblob = mfv_transval_replace(transblob, newdatum, i);
                transval = (mfvtransval *)VARDATA(transblob);
                transval->mfvs[i].cnt = tmpcnt;
                break;
            }
            /* else this is not a frequent value */
        }
    }
    PG_RETURN_DATUM(PointerGetDatum(transblob));
}

/*!
 * look to see if the mfvsketch currently has <c>val</c>
 * stored as one of its most-frequent values.
 * Returns the offset in the <c>mfvs</c> array, or -1
 * if not found.
 * NOTE: a 0 return value means the item <i>was found</i>
 * at offset 0!
 * \param blob a bytea holding an mfv transval
 * \param val the datum to search for
 */
int mfv_find(bytea *blob, Datum val)
{
    mfvtransval *transval = (mfvtransval *)VARDATA(blob);
    unsigned     i;
    uint32       len;
    void *       tmp;
    Datum        iDat;

    /* look for existing entry for this value */
    for (i = 0; i < transval->next_mfv; i++) {
        /* if they're the same */
        tmp = mfv_transval_getval(blob,i);
        iDat = MFVPointerGetDatum(tmp, transval->typByVal);

        if ((len = (uint32)att_addlength_datum(0, transval->typLen, iDat))
            == (uint32)att_addlength_datum(0, transval->typLen, val)) {
            void *valp, *datp;
            if (transval->typByVal) {
                valp = (void *)&val;
                datp = (void *)&iDat;
            }
            else {
                valp = (void *)DatumGetPointer(val);
                datp = (void *)DatumGetPointer(iDat);
            }
            if (!memcmp(datp, valp, len))
                /* arg is an mfv */
                return(i);
        }
    }
    return(-1);
}

/*!
 * Initialize an mfv sketch
 * \param max_mfvs the number of "bins" in the histogram
 * \param typOid the type ID for the column
 */
bytea *mfv_init_transval(int max_mfvs, Oid typOid)
{
    int          initial_size;
    bool         typIsVarLen;
    bytea *      transblob;
    mfvtransval *transval;

    /*
     * initialize mfvtransval, using palloc0 to zero it out.
     * if typlen is positive (fixed), size chosen accurately.
     * Else we'll do a conservative estimate of 16 bytes, and repalloc as needed.
     */
    if ((initial_size = get_typlen(typOid)) > 0)
        initial_size *= max_mfvs*get_typlen(typOid);
    else /* guess */
        initial_size = max_mfvs*16;

    transblob = (bytea *)palloc0(MFV_TRANSVAL_SZ(max_mfvs) + initial_size);

    SET_VARSIZE(transblob, MFV_TRANSVAL_SZ(max_mfvs) + initial_size);
    transval = (mfvtransval *)VARDATA(transblob);
    transval->max_mfvs = max_mfvs;
    transval->next_mfv = 0;
    transval->next_offset = MFV_TRANSVAL_SZ(max_mfvs)-VARHDRSZ;
    transval->typOid = typOid;
    getTypeOutputInfo(transval->typOid,
                      &(transval->outFuncOid),
                      &(typIsVarLen));
    transval->typLen = get_typlen(transval->typOid);
    transval->typByVal = get_typbyval(transval->typOid);
    if (!transval->outFuncOid) {
        /* no outFunc for this type! */
        elog(ERROR, "no outFunc for type %d", transval->typOid);
    }
    return(transblob);
}

/*! 
 * get the datum associated with the i'th mfv 
 * \param blob a bytea holding an mfv transval
 * \param i index of the mfv to look up
 */
void *mfv_transval_getval(bytea *blob, uint32 i)
{
    mfvtransval *tvp = (mfvtransval *)VARDATA(blob);
    void *       retval = (void *)(((char*)tvp) + tvp->mfvs[i].offset);
    Datum        dat = MFVPointerGetDatum(retval, tvp->typByVal);

    if (i > tvp->next_mfv || i < 0)
        elog(ERROR,
             "attempt to get frequent value at illegal index %d in mfv sketch",
             i);
    if (tvp->mfvs[i].offset > VARSIZE(blob) - VARHDRSZ
        || tvp->mfvs[i].offset < MFV_TRANSVAL_SZ(tvp->max_mfvs)-VARHDRSZ)
        elog(ERROR, "illegal offset %u in mfv sketch", tvp->mfvs[i].offset);
    if (tvp->mfvs[i].offset  + att_addlength_datum(0, tvp->typLen, dat)
        > VARSIZE(blob) - VARHDRSZ)
        elog(ERROR, "value overruns size of mfv sketch");

    return (retval);
}

/*!
 * copy datum <c>dat</c> into the offset of position <c>index</c> of
 * the mfv sketch stored in <c>transblob</c>.
 *
 * <i>Caller beware: this helper return assumes that 
 * <c>dat</c> is small enough to fit in the storage
 * currently used by the datum at position <c>index</c>.</i>
 *
 * \param transblob a bytea holding and mfv transval
 * \param index the index of the destination for copying
 * \param dat the datum to be copied into the transval
 */
void mfv_copy_datum(bytea *transblob, int index, Datum dat)
{
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    size_t       datumLen = att_addlength_datum(0, transval->typLen, dat);
    void *       curval = mfv_transval_getval(transblob,index);

    if (transval->typByVal)
        memmove(curval, (void *)&dat, datumLen);
    else
        memmove(curval, (void *)DatumGetPointer(dat), datumLen);
}

/*!
 * insert a value at position i of the mfv sketch
 *
 * we do not overwrite the previous value at position i.
 * instead we place the new value at the next_offset. 
 * 
 * <i>Note: we do not currently garbage collection the old value's storage.
 * This wastes space, with the worst-case scenario being a column with values
 * of increasing size and frequency!</i>
 *
 * \param transblob the transition value packed into a bytea
 * \param dat the value to be inserted
 * \param i the position to insert at
 */
bytea *mfv_transval_insert_at(bytea *transblob, Datum dat, uint32 i)
{
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    bytea *      tmpblob;
    size_t       datumLen = att_addlength_datum(0, transval->typLen, dat);

    if (i > transval->next_mfv || i < 0)
        elog(
            ERROR,
            "attempt to insert frequent value at illegal index %d in mfv sketch",
            i);
    if (MFV_TRANSVAL_CAPACITY(transblob) < datumLen) {
        /* allocate a copy with room for this, and double the current space for values */
        size_t curspace = VARSIZE(transblob) - transval->mfvs[0].offset -
                          VARHDRSZ;
        tmpblob = (bytea *)palloc0(VARSIZE(transblob) + curspace + datumLen);
        memmove(tmpblob, transblob, VARSIZE(transblob));
        SET_VARSIZE(tmpblob, VARSIZE(transblob) + curspace + datumLen);
        /*
         * PG won't let us pfree the old transblob
         * pfree(transblob);
         */
        transblob = tmpblob;
        transval = (mfvtransval *)VARDATA(transblob);
    }
    transval->mfvs[i].offset = transval->next_offset;
    mfv_copy_datum(transblob, i, dat);

    transval->next_offset += datumLen;

    return(transblob);
}

/*!
 * insert a value into the mfvsketch
 * \param transblob the transition value packed into a bytea
 * \param dat the value to be inserted
 */
bytea *mfv_transval_append(bytea *transblob, Datum dat)
{
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    bytea *      retval;

    if (transval->next_mfv == transval->max_mfvs) {
        elog(ERROR, "attempt to append to a full mfv sketch");
    }
    retval = mfv_transval_insert_at(transblob,
                                    dat,
                                    transval->next_mfv);
    (((mfvtransval *)VARDATA(retval))->next_mfv)++;

    return(retval);
}

/*!
 * replace the value at position i of the mfvsketch with dat
 *
 * \param transblob the transition value packed into a bytea
 * \param dat the value to be inserted
 * \param i the position to replace
 */
bytea *mfv_transval_replace(bytea *transblob, Datum dat, int i)
{
    /*
     * if new value is smaller than old, we overwrite at the old offset.
     * otherwise we call mfv_transval_insert_at which will take care of
     * space allocation for the new value
     */
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    size_t       datumLen = att_addlength_datum(0, transval->typLen, dat);
    void *       tmpp = mfv_transval_getval(transblob,i);
    Datum        oldDat = MFVPointerGetDatum(tmpp, transval->typByVal);
    size_t       oldLen = att_addlength_datum(0, transval->typLen, oldDat);

    if (datumLen <= oldLen) {
        mfv_copy_datum(transblob, i, dat);
        return transblob;
    }
    else return(mfv_transval_insert_at(transblob, dat, i));
}

PG_FUNCTION_INFO_V1(__mfvsketch_final);
/*!
 * scalar function taking an mfv sketch, returning a histogram of
 * its most frequent values
 */
Datum __mfvsketch_final(PG_FUNCTION_ARGS)
{
    bytea *      transblob = PG_GETARG_BYTEA_P(0);
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    ArrayType *  retval;
    uint32       i;
    Datum        histo[transval->max_mfvs][2];
    int          dims[2], lbs[2];
    /* Oid     typInput, typIOParam; */
    Oid          outFuncOid;
    bool         typIsVarlena;
    int16        typlen;
    bool         typbyval;
    char         typalign;
    char         typdelim;
    Oid          typioparam;
    Oid          typiofunc;


    if (PG_ARGISNULL(0)) PG_RETURN_NULL();
    if (VARSIZE(transblob) < MFV_TRANSVAL_SZ(0)) PG_RETURN_NULL();

    transval = (mfvtransval *)VARDATA(transblob);

    qsort(transval->mfvs, transval->next_mfv, sizeof(offsetcnt), cnt_cmp_desc);
    getTypeOutputInfo(INT8OID,
                      &outFuncOid,
                      &typIsVarlena);

    for (i = 0; i < transval->next_mfv; i++) {
        void *tmpp = mfv_transval_getval(transblob,i);
        Datum curval = MFVPointerGetDatum(tmpp, transval->typByVal);
        char *countbuf =
            OidOutputFunctionCall(outFuncOid,
                                  Int64GetDatum(transval->mfvs[i].cnt));
        char *valbuf = OidOutputFunctionCall(transval->outFuncOid, curval);
        
        histo[i][0] = PointerGetDatum(cstring_to_text(valbuf));
        histo[i][1] = PointerGetDatum(cstring_to_text(countbuf));
        pfree(countbuf);
        pfree(valbuf);
    }

    /*
     * Get info about element type
     */
    get_type_io_data(TEXTOID, IOFunc_output,
                     &typlen, &typbyval,
                     &typalign, &typdelim,
                     &typioparam, &typiofunc);

    dims[0] = i;
    dims[1] = 2;
    lbs[0] = lbs[1] = 0;
    retval = construct_md_array((Datum *)histo,
                                NULL,
                                2,
                                dims,
                                lbs,
                                TEXTOID,
                                -1,
                                0,
                                'i');
    PG_RETURN_ARRAYTYPE_P(retval);
}



/*!
 * support function to sort by count
 * \param i an offsetcnt object cast to a (void *)
 * \param j an offsetcnt object cast to a (void *)
 */
int cnt_cmp_desc(const void *i, const void *j)
{
    offsetcnt *o = (offsetcnt *)i;
    offsetcnt *p = (offsetcnt *)j;

    return (p->cnt - o->cnt);
}


/*!
 * Greenplum "prefunc" to combine sketches from multiple machines.
 * See notes at top of file regarding the heuristic nature of this approach.
 */
PG_FUNCTION_INFO_V1(__mfvsketch_merge);
Datum __mfvsketch_merge(PG_FUNCTION_ARGS)
{
    bytea * transblob1 = (bytea *)PG_GETARG_BYTEA_P(0);
    bytea * transblob2 = (bytea *)PG_GETARG_BYTEA_P(1);

    PG_RETURN_DATUM(PointerGetDatum(mfvsketch_merge_c(transblob1, transblob2)));
}

/*!
 * implementation of the merge of two mfv sketches.  we
 * first merge the embedded countmin sketches to get the
 * sums of the counts, and then use those sums to pick the
 * top values for the resulting histogram.  We overwrite
 * the first argument and return it.
 * \param transblob1 an mfv transval stored inside a bytea
 * \param transblob2 another mfv transval in a bytea
 */
bytea *mfvsketch_merge_c(bytea *transblob1, bytea *transblob2)
{
    mfvtransval *transval1 = (mfvtransval *)VARDATA(transblob1);
    mfvtransval *transval2 = (mfvtransval *)VARDATA(transblob2);
    uint32       i, j;

    /* handle uninitialized args */
    if (VARSIZE(transblob1) <= sizeof(MFV_TRANSVAL_SZ(0))
        && VARSIZE(transblob2) <= sizeof(MFV_TRANSVAL_SZ(0)))
        return(transblob1);
    else if (VARSIZE(transblob1) <= sizeof(MFV_TRANSVAL_SZ(0))) {
        transblob1 = mfv_init_transval(transval2->max_mfvs, transval2->typOid);
        transval1 = (mfvtransval *)VARDATA(transblob1);
    }
    else if (VARSIZE(transblob2) <= sizeof(MFV_TRANSVAL_SZ(0))) {
        transblob2 = mfv_init_transval(transval1->max_mfvs, transval1->typOid);
        transval2 = (mfvtransval *)VARDATA(transblob2);
    }

    /* combine sketches */
    for (i = 0; i < DEPTH; i++)
        for (j = 0; j < NUMCOUNTERS; j++)
            transval1->sketch[i][j] += transval2->sketch[i][j];

    /* recompute the counts using the merged sketch */
    for (i = 0; i < transval1->next_mfv; i++) {
        void *tmpp = mfv_transval_getval(transblob1,i);
        Datum dat = MFVPointerGetDatum(tmpp, transval1->typByVal);

        transval1->mfvs[i].cnt = cmsketch_count_c(transval1->sketch,
                                                  dat,
                                                  transval1->outFuncOid);
    }
    for (i = 0; i < transval2->next_mfv; i++) {
        void *tmpp = mfv_transval_getval(transblob2,i);
        Datum dat = MFVPointerGetDatum(tmpp, transval2->typByVal);

        transval2->mfvs[i].cnt = cmsketch_count_c(transval2->sketch,
                                                  dat,
                                                  transval2->outFuncOid);
    }

    /* now take maxes on mfvs in a sort-merge style, copying into transval1  */
    qsort(transval1->mfvs, transval1->next_mfv, sizeof(offsetcnt), cnt_cmp_desc);
    qsort(transval2->mfvs, transval2->next_mfv, sizeof(offsetcnt), cnt_cmp_desc);

    /* scan through transval1, replace as we find bigger things in transval2 */
    for (i = j = 0;
         j < transval2->next_mfv && i < transval1->max_mfvs;
         i++) {
        void *tmpp = mfv_transval_getval(transblob2, j);
        Datum jDatum = MFVPointerGetDatum(tmpp, transval2->typByVal);

        if (i == transval1->next_mfv && (mfv_find(transblob1, jDatum) == -1)
            /* && i < transval1->max_mfvs from for loop */) {
            transblob1 = mfv_transval_append(transblob1, jDatum);
            transval1 = (mfvtransval *)VARDATA(transblob1);
            j++;
        }
        else if (transval1->mfvs[i].cnt < transval2->mfvs[j].cnt
                 && mfv_find(transblob1, jDatum) == -1) {
            /* copy into transval1 and advance both  */
            transblob1 = mfv_transval_replace(transblob1, jDatum, i);
            transval1 = (mfvtransval *)VARDATA(transblob1);
            j++;
        }
    }
    return(transblob1);
}
