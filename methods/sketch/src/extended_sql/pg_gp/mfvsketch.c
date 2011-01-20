/*! 
 * \file mfvsketch.c
 *
 * \brief CountMin sketch for Most Frequent Value estimation
 */
 
 /*!
 * \defgroup mfvsketch MFV Sketch
 * \ingroup countmin
 * \par About
 * MFVSketch: Most Frequent Values variant of CountMin sketch.
 * This is basically a CountMin sketch that keeps track of most frequent values
 * as it goes.
 * It only needs to do cmsketch_count, doesn't need the "dyadic" range trick.
 * As a result it's not limited to integers, and the implementation works
 * for the Postgres "anyelement".
 *
 * \par Usage/API:
 *
 *  - <c>mfvsketch_top_histogram(col anytype, nbuckets int4)</c>          is a UDA over column <c>col</c> of any type, and a number of buckets <c>nbuckets</c>, and produces an n-bucket histogram for the column where each bucket is for one of the most frequent values in the column. The output is an array of doubles {value, count} in descending order of frequency; counts are approximate. Ties are handled arbitrarily.  Example:\code
 *   SELECT pronamespace, madlib.mfvsketch_top_histogram(pronargs, 4)
 *     FROM pg_proc
 * GROUP BY pronamespace;
 * \endcode
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
    int          num_mfvs  = PG_GETARG_INT32(2);
    mfvtransval *transval;
    int64        tmpcnt;
    int          i;
    bool         typIsVarlena;
    bool         found = false;
    char *       outString;
    bytea *      outText;

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

    /* ignore NULL inputs */
    if (PG_ARGISNULL(1) || PG_ARGISNULL(2))
        PG_RETURN_DATUM(PointerGetDatum(transblob));

    transval = (mfvtransval *)(VARDATA(transblob));

    if (VARSIZE(transblob) <= sizeof(MFV_TRANSVAL_SZ(0))) {
        Oid typOid = get_fn_expr_argtype(fcinfo->flinfo, 1);
        int initial_size;
        /*
         * initialize mfvtransval, using palloc0 to zero it out.
         * if typlen is positive (fixed), size chosen large enough to hold
         * one 3x the length (on the theory that 2^8=256 takes 3 chars as a string).
         * Else we'll do a conservative estimate of 8 bytes (=24 chars), and repalloc as needed.
         */
        if ((initial_size = get_typlen(typOid)) > 0)
            initial_size *= num_mfvs*3;
        else /* guess */
            initial_size = num_mfvs*24;

        transblob = (bytea *)palloc0(MFV_TRANSVAL_SZ(num_mfvs) + initial_size);

        SET_VARSIZE(transblob, MFV_TRANSVAL_SZ(num_mfvs) + initial_size);
        transval = (mfvtransval *)VARDATA(transblob);
        transval->num_mfvs = num_mfvs;
        transval->next_mfv = 0;
        transval->next_offset = MFV_TRANSVAL_SZ(num_mfvs)-VARHDRSZ;
        transval->typOid = typOid;
        getTypeOutputInfo(transval->typOid,
                          &(transval->outFuncOid),
                          &typIsVarlena);
        if (!transval->outFuncOid) {
            /* no outFunc for this type! */
            elog(ERROR, "no outFunc for type %d", transval->typOid);
        }
    }

    transval = (mfvtransval *)VARDATA(transblob);
    /* insert into the countmin sketch */
    outString = countmin_trans_c(transval->sketch, PG_GETARG_DATUM(
                                     1), transval->outFuncOid);
    outText = cstring_to_text(outString);

    tmpcnt = cmsketch_count_c(transval->sketch,
                              PG_GETARG_DATUM(1),
                              transval->outFuncOid);
    /* look for existing entry for this value */
    for (i = 0; i < transval->next_mfv; i++) {
        bytea *iText = mfv_transval_getval(transval,i);
        /* if they're the same */
        if (VARSIZE(iText) == VARSIZE(outText)
            && !strncmp(VARDATA(iText), VARDATA(outText), VARSIZE(iText)-
                        VARHDRSZ)) {
            /* arg is an mfv */
            transval->mfvs[i].cnt = tmpcnt;
            found = true;
            break;
        }
    }
    if (!found)
        /* try to insert as either a new or replacement entry */
        for (i = 0; i < transval->num_mfvs; i++) {
            if ((i == transval->next_mfv)) {
                /* room for new */
                transblob = mfv_transval_insert(transblob, outText);
                transval = (mfvtransval *)VARDATA(transblob);
                transval->mfvs[i].cnt = tmpcnt;
                break;
            }
            else if (transval->mfvs[i].cnt < tmpcnt) {
                /* arg beats this mfv */
                transblob = mfv_transval_replace(transblob, outText, i);
                transval = (mfvtransval *)VARDATA(transblob);
                transval->mfvs[i].cnt = tmpcnt;
                break;
            }
            /* else this is not a frequent value */
        }
    pfree(outText);
    PG_RETURN_DATUM(PointerGetDatum(transblob));
}
/*!
 * insert a value at position i of the mfv sketch
 * \param transblob the transition value packed into a bytea
 * \param text the value to be inserted
 * \param i the position to insert at
 */
bytea *mfv_transval_insert_at(bytea *transblob, bytea *text, int i)
{
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    bytea *      tmpblob;

    if (MFV_TRANSVAL_CAPACITY(transblob) < VARSIZE(text)) {
        /* allocate a copy with double the current space for values */
        size_t curspace = transval->next_offset - transval->mfvs[0].offset;
        tmpblob = palloc0(VARSIZE(transblob) + curspace);
        memcpy(tmpblob, transblob, VARSIZE(transblob));
        SET_VARSIZE(tmpblob, VARSIZE(transblob) + curspace);
        /*
         * PG won't let us pfree the old transblob
         * pfree(transblob);
         */
        transblob = tmpblob;
        transval = (mfvtransval *)VARDATA(transblob);
    }
    transval->mfvs[i].offset = transval->next_offset;
    memcpy(mfv_transval_getval(transval,i), (char *)text, VARSIZE(text));
    transval->next_offset += VARSIZE(text);
    if (i == transval->next_mfv)
        (transval->next_mfv)++;

    return(transblob);
}

/*!
 * insert a value into the mfvsketch
 * \param transblob the transition value packed into a bytea
 * \param text the value to be inserted
 */
bytea *mfv_transval_insert(bytea *transblob, bytea *text)
{
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    bytea *      retval = mfv_transval_insert_at(transblob,
                                                 text,
                                                 transval->next_mfv);

    return(retval);
}

/*!
 * replace the value at position i of the mfvsketch with text
 * \param transblob the transition value packed into a bytea
 * \param text the value to be inserted
 * \param i the position to replace
 */
bytea *mfv_transval_replace(bytea *transblob, bytea *text, int i)
{
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    /* bytea *tmpblob; */

    if (VARSIZE(text) < VARSIZE((bytea *)mfv_transval_getval(transval,i))) {
        memcpy(mfv_transval_getval(transval, i), (char *)text, VARSIZE(text));
        return transblob;
    }
    else return(mfv_transval_insert_at(transblob, text, i));
}

PG_FUNCTION_INFO_V1(__mfvsketch_final);
/*!
 * scalar function taking an mfv sketch, returning a string with
 * its most frequent values
 */
Datum __mfvsketch_final(PG_FUNCTION_ARGS)
{
    bytea *      transblob = PG_GETARG_BYTEA_P(0);
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    bytea *      retval;
    int          i;
    /* longest number takes MAXINT8LEN characters, plus two for punctuation,
     * plus 1 for comfort */
    char         numbuf[MAXINT8LEN+3];

    if (PG_ARGISNULL(0)) PG_RETURN_NULL();
    if (VARSIZE(transblob) < MFV_TRANSVAL_SZ(0)) PG_RETURN_NULL();

    /*
     * we need MAXINT8LEN characters per counter and value,
     * and we need padding for 5 punctuation per counter
     * plus 1 for comfort
     */
    retval = palloc(VARSIZE(
                        transblob) + transval->num_mfvs*(2*MAXINT8LEN+5) + 1);
    SET_VARSIZE(retval, VARHDRSZ);

    qsort(transval->mfvs, transval->next_mfv, sizeof(offsetcnt), cnt_cmp_desc);

    for (i = 0; i < transval->next_mfv; i++) {
        if (i > 0) {
            ((char *)retval)[VARSIZE(retval)] = ' ';
            SET_VARSIZE(retval, VARSIZE(retval)+1);
        }
        ((char *)retval)[VARSIZE(retval)] = '[';
        SET_VARSIZE(retval, VARSIZE(retval)+1);
        memcpy(&(((char *)retval)[VARSIZE(retval)]),
               VARDATA(mfv_transval_getval(transval,i)),
               VARSIZE(mfv_transval_getval(transval,i)) - VARHDRSZ);
        SET_VARSIZE(retval, VARSIZE(retval) +
                    VARSIZE(mfv_transval_getval(transval,i)) - VARHDRSZ);
        sprintf(numbuf, ": ");
        sprintf(&(numbuf[2]), UINT64_FORMAT, transval->mfvs[i].cnt);
        memcpy(&(((char *)retval)[VARSIZE(retval)]), numbuf, strlen(numbuf));
        SET_VARSIZE(retval, VARSIZE(retval) + strlen(numbuf));
        ((char *)retval)[VARSIZE(retval)] = ']';
        SET_VARSIZE(retval, VARSIZE(retval)+1);
    }
    PG_RETURN_BYTEA_P(retval);
}

PG_FUNCTION_INFO_V1(mfvsketch_array_out);
/*!
 * scalar function taking an mfv sketch, returning an array of tuple types
 * for its most frequent values.
 * XXX this code is under development and not working, should not be used.
 */
Datum mfvsketch_array_out(PG_FUNCTION_ARGS)
{
    bytea *      transblob = PG_GETARG_BYTEA_P(0);
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    int          i;
    Datum        pair[2];
    Oid          resultTypeId, elemTypeId;
    TupleDesc    resultTupleDesc;
    bool         isNull;
    Datum *      result_data;
    ArrayType *  retval;


    if (PG_ARGISNULL(0)) PG_RETURN_NULL();
    if (VARSIZE(transblob) < MFV_TRANSVAL_SZ(0)) PG_RETURN_NULL();

    result_data = (Datum *)palloc(sizeof(Datum) * transval->next_mfv);
    /*
     * the type we return is an array of records.  We need a tupledesc for the
     * elements of this array.
     */
    get_call_result_type(fcinfo, &resultTypeId, &resultTupleDesc);
    elemTypeId = get_element_type(resultTypeId);
    resultTupleDesc = lookup_rowtype_tupdesc_copy(elemTypeId, -1);
    BlessTupleDesc(resultTupleDesc);

    qsort(transval->mfvs, transval->next_mfv, sizeof(offsetcnt), cnt_cmp_desc);

    for (i = 0; i < transval->next_mfv; i++) {
        pair[0] = PointerGetDatum(mfv_transval_getval(transval,i));
        pair[1] = Int64GetDatum(transval->mfvs[0].cnt);

        result_data[i] =
            HeapTupleGetDatum(heap_form_tuple(resultTupleDesc, pair, &isNull));
    }
    retval = construct_array((Datum *)result_data,
                             transval->next_mfv,
                             elemTypeId,
                             sizeof(Datum),
                             false,
                             'd');

    PG_RETURN_BYTEA_P(retval);
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
