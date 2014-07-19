/*!
 * \file mfvsketch.c

 \brief CountMin sketch for Most Frequent Value estimation
 \implementation
 This is basically a CountMin sketch that keeps track of most frequent values
 as it goes.  This is easy to do, because at any point during a scan,
 it can use the CM sketch to quickly get the count of any value so far.

 It only uses CountMin sketches for value counting, and doesn't need the "dyadic" range trick.
 As a result it's not limited to integers, and the implementation works
 for any Postgres data type.


 The parallel method (<c>mfvsketch_quick_histogram</c>) is a heuristic with no
 such guarantees, but it will likely work well in most cases.  As an example
 of a case where it will fail, consider a scenario where the top <i>n</i> values on node 1 are very infrequent on
 node 2, and the top <i>n</i> values on node 2 are infrequent on node 1.  But the <i>n</i>+1'th value
 is the same on both nodes and the most frequent value in toto.  It will get
 surpressed incorrectly by the parallel heuristic, but get chosen by the standard method.

 However, we're probably OK here most of the time.  What we're interested in are
 values whose frequencies are unusually high. For
  columns with very flat distributions, we likely don't care about the results much.
  Otherwise, The results of this heuristic will likely be
  unusually frequent values, if not precisely the *most* frequent values.
 */



#include <postgres.h>
#include <utils/array.h>
#include <utils/elog.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>
#include <utils/numeric.h>
#include <utils/typcache.h>
#include <nodes/execnodes.h>
#include <fmgr.h>
#include <catalog/pg_type.h>
#include <funcapi.h>
#include "sketch_support.h"
#include "countmin.h"

#include <ctype.h>

/* check whether the content in the given bytea is safe for mfvtransval */
void check_mfvtransval(bytea *storage) {
    size_t left_len = VARSIZE(storage);
    Oid     outFuncOid;
    bool    typIsVarLen;

    mfvtransval *mfv  = NULL;

    if (left_len < VARHDRSZ + sizeof(mfvtransval)) {
        elog(ERROR, "invalid transition state for mfvsketch");
    }
    mfv = (mfvtransval*)VARDATA(storage);
    left_len -= VARHDRSZ + sizeof(mfvtransval);

    if (mfv->next_mfv > mfv->max_mfvs) {
        elog(ERROR, "invalid transition state for mfvsketch");
    }

    if (mfv->next_offset + VARHDRSZ > VARSIZE(storage)) {
        elog(ERROR, "invalid transition state for mfvsketch");
    }

    if (InvalidOid == mfv->typOid) {
        elog(ERROR, "invalid transition state for mfvsketch");
    }

    getTypeOutputInfo(mfv->typOid, &outFuncOid, &typIsVarLen);
    if (mfv->outFuncOid != outFuncOid
        || mfv->typLen != get_typlen(mfv->typOid)
        || mfv->typByVal != get_typbyval(mfv->typOid)) {
        elog(ERROR, "invalid transition state for mfvsketch");
    }

    if (left_len < sizeof(offsetcnt)*mfv->max_mfvs) {
        elog(ERROR, "invalid transition state for mfvsketch");
    }
}

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
    Datum        md5_datum;

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
    else {
        check_mfvtransval(transblob);
    }

    /* ignore NULL inputs */
    if (PG_ARGISNULL(1) || PG_ARGISNULL(2))
        PG_RETURN_DATUM(PointerGetDatum(transblob));

    transval = (mfvtransval *)VARDATA(transblob);
    if (transval->typOid != get_fn_expr_argtype(fcinfo->flinfo, 1)) {
        elog(ERROR, "cannot aggregate on elements with different types");
    }
    /* insert into the countmin sketch */
    md5_datum = countmin_trans_c(transval->sketch,
                                newdatum,
                                transval->outFuncOid,
                                transval->typOid);

    tmpcnt = cmsketch_count_md5_datum(transval->sketch,
                                      (bytea *)DatumGetPointer(md5_datum),
                                      transval->outFuncOid);
    i = mfv_find(transblob, newdatum);

    if (i > -1) {
        transval->mfvs[i].cnt = tmpcnt;
    }
    else {
        /* try to insert as either a new or replacement entry */
        for (i = 0; i < (int)transval->max_mfvs; i++) {
            if (i == (int)transval->next_mfv) {
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
    void *       datp;
    Datum        iDat;
    void        *valp = DatumExtractPointer(val, transval->typByVal);

    /* look for existing entry for this value */
    for (i = 0; i < transval->next_mfv; i++) {
        /* if they're the same */
        datp = mfv_transval_getval(blob,i);
        iDat = PointerExtractDatum(datp, transval->typByVal);

        if ((len = ExtractDatumLen(iDat, transval->typLen, transval->typByVal, -1))
            == ExtractDatumLen(val, transval->typLen, transval->typByVal, -1)) {
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
 * \param blob a bytea holding an mfv transval
 * \param i index of the mfv to look up
 * \returns pointer to the datum associated with the i'th mfv
 */
void *mfv_transval_getval(bytea *blob, uint32 i)
{
    mfvtransval *tvp = (mfvtransval *)VARDATA(blob);
    void *       retval = (void *)(((char*)tvp) + tvp->mfvs[i].offset);
    Datum        dat = PointerExtractDatum(retval, tvp->typByVal);

    if (i >= tvp->next_mfv)
        elog(ERROR,
             "attempt to get frequent value at illegal index %d in mfv sketch",
             i);
    if (tvp->mfvs[i].offset > VARSIZE(blob) - VARHDRSZ
        || tvp->mfvs[i].offset < MFV_TRANSVAL_SZ(tvp->max_mfvs)-VARHDRSZ)
        elog(ERROR, "illegal offset %u in mfv sketch", tvp->mfvs[i].offset);
    /*
     * call ExtractDatumLen to make sure enough space, this checking is unnecessary,
     * it is used to prevent gcc from optimizing out the ExtractDatumLen function call.
     */
    if (tvp->mfvs[i].offset
        + ExtractDatumLen(dat, tvp->typLen, tvp->typByVal, VARSIZE(blob) - VARHDRSZ - tvp->mfvs[i].offset)
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
    size_t       datumLen = ExtractDatumLen(dat, transval->typLen, transval->typByVal, -1);
    void *       curval = (char*)transval +  transval->mfvs[index].offset;

    memmove(curval, (void *)DatumExtractPointer(dat, transval->typByVal), datumLen);
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
    size_t       datumLen = ExtractDatumLen(dat, transval->typLen, transval->typByVal, -1);

    if (i > transval->next_mfv)
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
    size_t       datumLen = ExtractDatumLen(dat, transval->typLen, transval->typByVal, -1);
    void *       tmpp = mfv_transval_getval(transblob,i);
    Datum        oldDat = PointerExtractDatum(tmpp, transval->typByVal);
    size_t       oldLen = ExtractDatumLen(oldDat, transval->typLen, transval->typByVal, -1);

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
    mfvtransval *transval = NULL;
    ArrayType *  retval;
    uint32       i;
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

    check_mfvtransval(transblob);
    transval = (mfvtransval *)VARDATA(transblob);
    /*
     * We only declare the variable-length array histo here after some sanity
     * checking. We risk a stack overflow otherwise. In particular, we need to
     * make sure that transval->max_mfvs is initialized. It might not be if the
     * (strict) transition function is never called. (MADLIB-254)
     */
    Datum        histo[transval->max_mfvs][2];

    qsort(transval->mfvs, transval->next_mfv, sizeof(offsetcnt), cnt_cmp_desc);
    getTypeOutputInfo(INT8OID,
                      &outFuncOid,
                      &typIsVarlena);

    for (i = 0; i < transval->next_mfv; i++) {
        void *tmpp = mfv_transval_getval(transblob,i);
        Datum curval = PointerExtractDatum(tmpp, transval->typByVal);
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
    void        *newblob;
    mfvtransval *newval;
    uint32       i, j, cnt;

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
    check_mfvtransval(transblob1);
    check_mfvtransval(transblob2);

    if ( transval1->typOid != transval2->typOid ) {
        elog(ERROR, "cannot merge two transition state with different element type");
    }

    /* initialize output */
    newblob   = mfv_init_transval(transval1->max_mfvs, transval1->typOid);
    newval    = (mfvtransval *)VARDATA(newblob);

    /* combine sketches */
    for (i = 0; i < DEPTH; i++)
        for (j = 0; j < NUMCOUNTERS; j++)
            newval->sketch[i][j] = transval1->sketch[i][j]
                                   + transval2->sketch[i][j];

    /* recompute the counts using the merged sketch */
    for (i = 0; i < transval1->next_mfv; i++) {
        void *tmpp = mfv_transval_getval(transblob1,i);
        Datum dat = PointerExtractDatum(tmpp, transval1->typByVal);

        transval1->mfvs[i].cnt = cmsketch_count_c(newval->sketch,
                                                  dat,
                                                  newval->outFuncOid,
                                                  newval->typOid);
    }
    for (i = 0; i < transval2->next_mfv; i++) {
        void *tmpp = mfv_transval_getval(transblob2,i);
        Datum dat = PointerExtractDatum(tmpp, transval2->typByVal);

        transval2->mfvs[i].cnt = cmsketch_count_c(newval->sketch,
                                                  dat,
                                                  newval->outFuncOid,
                                                  newval->typOid);
    }

    /* now take maxes on mfvs in a sort-merge style, copying into transval1  */
    qsort(transval1->mfvs, transval1->next_mfv, sizeof(offsetcnt), cnt_cmp_desc);
    qsort(transval2->mfvs, transval2->next_mfv, sizeof(offsetcnt), cnt_cmp_desc);

    /* choose top k from transval1 and transval2 */
    for (i = j = cnt = 0;
         cnt < newval->max_mfvs
         && (j < transval2->next_mfv || i < transval1->next_mfv);
         cnt++) {
        Datum iDatum, jDatum;

    if (i < transval1->next_mfv &&
            (j == transval2->next_mfv
             || transval1->mfvs[i].cnt >= transval2->mfvs[j].cnt)) {
          /* next item comes from transval1 */
          iDatum = PointerExtractDatum(mfv_transval_getval(transblob1, i),
                                       transval1->typByVal);
          newblob = mfv_transval_append(newblob, iDatum);
          newval = (mfvtransval *)VARDATA(newblob);
          newval->mfvs[cnt].cnt = transval1->mfvs[i].cnt;
          i++;
        }
        else if (j < transval2->next_mfv &&
                 (i == transval1->next_mfv
                  || transval1->mfvs[i].cnt < transval2->mfvs[j].cnt)) {
          /* next item comes from transval2 */
          jDatum = PointerExtractDatum(mfv_transval_getval(transblob2, j),
                                       transval2->typByVal);
          newblob = mfv_transval_append(newblob, jDatum);
          newval = (mfvtransval *)VARDATA(newblob);
          newval->mfvs[cnt].cnt = transval2->mfvs[j].cnt;
          j++;
        }
    }
    return(newblob);
}
