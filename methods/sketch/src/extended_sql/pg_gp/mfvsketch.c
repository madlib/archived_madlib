/*
 * MFVSketch: Most Frequent Values variant of CountMin sketch.
 * This is basically a CountMin sketch (see discussion in cmsketch.c)
 * that keeps track of most frequent values as it goes.  
 *
 * It only needs to do cmsketch_getcount, doesn't need the "dyadic" range trick.
 * As a result it's not limited to integers, and the implementation works
 * for the Postgres "anyelement".
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

PG_FUNCTION_INFO_V1(mfvsketch_trans);

/*
 *  transition function to maintain a CountMin sketch with 
 *  Most-Frequent Values
 */
Datum mfvsketch_trans(PG_FUNCTION_ARGS)
{
    bytea *      transblob = PG_GETARG_BYTEA_P(0);
    int          num_mfvs  = PG_GETARG_INT32(2);
    mfvtransval *transval;
    int64        tmpcnt;
    int          i;
    bool         typIsVarlena;
    bool         found = false;
    char         *outString;
    bytea        *outText;

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
    
    // elog(NOTICE, "got transblob at address %p from qp", transblob);
    transval = (mfvtransval *)(VARDATA(transblob));
    // elog(NOTICE, "entered trans with transblob of size %d", VARSIZE(transblob));    
      
    if (VARSIZE(transblob) < sizeof(MFV_TRANSVAL_SZ(0))) {
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
        // elog(NOTICE, "transblob moved to address %p", transblob);
        
        SET_VARSIZE(transblob, MFV_TRANSVAL_SZ(num_mfvs) + initial_size);
        // elog(NOTICE, "VARSIZE of transblob set to %u", VARSIZE(transblob));
        transval = (mfvtransval *)VARDATA(transblob);
        transval->num_mfvs = num_mfvs;
        transval->next_mfv = 0;
        transval->next_offset = MFV_TRANSVAL_SZ(num_mfvs)-VARHDRSZ;
        transval->typOid = typOid;
        getTypeOutputInfo(transval->typOid,
                           &(transval->outFuncOid),
                           &typIsVarlena);
    }
    
    transval = (mfvtransval *)VARDATA(transblob);
    /* insert into the countmin sketch */
    outString = countmin_trans_c(transval->sketch, PG_GETARG_DATUM(1), transval->outFuncOid);
    outText = cstring_to_text(outString);
                            
    tmpcnt = cmsketch_getcount_c(transval->sketch,
                                 PG_GETARG_DATUM(1),
                                 transval->outFuncOid);
    /* look for existing entry for this value */                        
    for (i = 0; i < transval->next_mfv; i++) {
        bytea *iText = mfv_transval_getval(transval,i);
        /* if they're the same */
        if (VARSIZE(iText) == VARSIZE(outText)
            && !strncmp(VARDATA(iText), VARDATA(outText), VARSIZE(iText)-VARHDRSZ)) {
            /* arg is an mfv */
                // elog(NOTICE, "-- SAME!");
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
                // elog(NOTICE, "trans validating string \"%s\" of length %u from position %d",
                //              VARDATA((char *)transblob + (transval->mfvs[i].offset)),
                //              VARSIZE((char *)transblob + (transval->mfvs[i].offset)),
                //              i);
                             
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
    // elog(NOTICE, "returning transblob at address %p to qp", transblob);
    PG_RETURN_DATUM(PointerGetDatum(transblob));
}

bytea *mfv_transval_insert_at(bytea *transblob, bytea *text, int i)
{
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    bytea *tmpblob;
    
    // elog(NOTICE, "capacity is %d, text size is %d", MFV_TRANSVAL_CAPACITY(transblob), VARSIZE(text));
    if (MFV_TRANSVAL_CAPACITY(transblob) < VARSIZE(text)) {
        /* allocate a copy with double the current space for values */
        size_t curspace = transval->next_offset - transval->mfvs[0].offset;
        // elog(NOTICE, "out of capacity, doubling from %d to %d!", curspace, curspace*2);
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
    // elog(NOTICE, "transblob at address %p", transblob);
    // elog(NOTICE, "got transblob with VARSIZE set to %u", VARSIZE(transblob));
    // elog(NOTICE, "inserting text \"%s\" of length %u at position %d (offset %d, address %p)", VARDATA(text), VARSIZE(text), i,
    //      transval->mfvs[i].offset, mfv_transval_getval(transval,i));
    memcpy(mfv_transval_getval(transval,i), (char *)text, VARSIZE(text));
    // elog(NOTICE, "validating string \"%s\" of length %u from position %d (offset %d, addr %p)",
    //       VARDATA(mfv_transval_getval(transval,0)),
    //       VARSIZE(mfv_transval_getval(transval,0)) - VARHDRSZ,
    //       0, transval->mfvs[0].offset, mfv_transval_getval(transval,0)); 
    transval->next_offset += VARSIZE(text);    
    if (i == transval->next_mfv) 
      (transval->next_mfv)++;

    // elog(NOTICE, "post-validating string \"%s\" of length %u from position %d (offset %d, addr %p)",
    //        VARDATA(mfv_transval_getval(transval,0)),
    //        VARSIZE(mfv_transval_getval(transval,0)) - VARHDRSZ,
    //        0, transval->mfvs[0].offset, mfv_transval_getval(transval,0));     
        
    return(transblob);
}

bytea *mfv_transval_insert(bytea *transblob, bytea *text)
{
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    bytea *retval = mfv_transval_insert_at(transblob, text, transval->next_mfv);
    
    return(retval);
}


bytea *mfv_transval_replace(bytea *transblob, bytea *text, int i)
{
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    // bytea *tmpblob;
    
    // elog(NOTICE, "replacing %d", i);
    if (VARSIZE(text) < VARSIZE((bytea *)mfv_transval_getval(transval,i))) {
        memcpy(mfv_transval_getval(transval, i), (char *)text, VARSIZE(text));
        return transblob;
    }
    else return(mfv_transval_insert_at(transblob, text, i));
}

PG_FUNCTION_INFO_V1(mfvsketch_out);
/*
 * scalar function taking an mfv sketch, returning a string with
 * of its most frequent values
 */
Datum mfvsketch_out(PG_FUNCTION_ARGS)
{
    bytea *      transblob = PG_GETARG_BYTEA_P(0);
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    bytea     *retval;
    int        i;
    char       numbuf[64];

 	if (PG_ARGISNULL(0)) PG_RETURN_NULL();
    if (VARSIZE(transblob) < MFV_TRANSVAL_SZ(0)) PG_RETURN_NULL();

    /* 
     * 2^64 ~ 1.84e+19, so 20 chars per counter should be enough,
     * and we need padding for punctuation, so to be safe we'll round
     * up to 32.
     */
    retval = palloc(VARSIZE(transblob) + transval->num_mfvs*32);
    SET_VARSIZE(retval, VARHDRSZ);
    
    qsort(transval->mfvs, transval->next_mfv, sizeof(offsetcnt), cnt_cmp_desc);
    
    for (i = 0; i < transval->next_mfv; i++) {
        if (i > 0) {
            ((char *)retval)[VARSIZE(retval)] = ' ';
            SET_VARSIZE(retval, VARSIZE(retval)+1);
        }
        ((char *)retval)[VARSIZE(retval)] = '[';
        SET_VARSIZE(retval, VARSIZE(retval)+1);
        memcpy(&(((char *)retval)[VARSIZE(retval)]), VARDATA(mfv_transval_getval(transval,i)),
               VARSIZE(mfv_transval_getval(transval,i)) - VARHDRSZ);
        SET_VARSIZE(retval, VARSIZE(retval) + VARSIZE(mfv_transval_getval(transval,i)) - VARHDRSZ);
        sprintf(numbuf, ": %ld", transval->mfvs[i].cnt);
        memcpy(&(((char *)retval)[VARSIZE(retval)]), numbuf, strlen(numbuf));
        SET_VARSIZE(retval, VARSIZE(retval) + strlen(numbuf));
        ((char *)retval)[VARSIZE(retval)] = ']';
        SET_VARSIZE(retval, VARSIZE(retval)+1);
    }
    PG_RETURN_BYTEA_P(retval);
}

PG_FUNCTION_INFO_V1(mfvsketch_array_out);
/*
 * scalar function taking an mfv sketch, returning an array of tuple types
 * for its most frequent values.  
 * XXX this code is under development and not working, should not be used.
 */
Datum mfvsketch_array_out(PG_FUNCTION_ARGS)
{
    bytea *      transblob = PG_GETARG_BYTEA_P(0);
    mfvtransval *transval = (mfvtransval *)VARDATA(transblob);
    int        i;
    Datum      pair[2];
    Oid       resultTypeId, elemTypeId;
    TupleDesc resultTupleDesc;
    bool       isNull;
    Datum     *result_data;
    ArrayType *retval;
    

 	if (PG_ARGISNULL(0)) PG_RETURN_NULL();
    if (VARSIZE(transblob) < MFV_TRANSVAL_SZ(0)) PG_RETURN_NULL();

    result_data = (Datum *)palloc(sizeof(Datum) * transval->next_mfv);
    /* 
     * the type we return is an array of records.  We need a tupledesc for the
     * elements of this array.
     */
    get_call_result_type(fcinfo, &resultTypeId, &resultTupleDesc);
    elog(NOTICE, "looking up type oid %d", resultTypeId);
    elemTypeId = get_element_type(resultTypeId);
    elog(NOTICE, "found type oid %d", elemTypeId);
    resultTupleDesc = lookup_rowtype_tupdesc_copy(elemTypeId, -1);
    BlessTupleDesc(resultTupleDesc);
    
    qsort(transval->mfvs, transval->next_mfv, sizeof(offsetcnt), cnt_cmp_desc);
    
    for (i = 0; i < transval->next_mfv; i++) {
        pair[0] = PointerGetDatum(mfv_transval_getval(transval,i));
        pair[1] = Int64GetDatum(transval->mfvs[0].cnt);
        
        result_data[i] = HeapTupleGetDatum(heap_form_tuple(resultTupleDesc, pair, &isNull));
    }
    elog(NOTICE, "constructing array");
    retval = construct_array((Datum *)result_data,
                             transval->next_mfv,
                             elemTypeId,
                             sizeof(Datum),
                             false,
                             'd');
    elog(NOTICE, "constructed");
    
    PG_RETURN_BYTEA_P(retval);
}

int cnt_cmp_desc(const void *i, const void *j)
{
    offsetcnt *o = (offsetcnt *)i;
    offsetcnt *p = (offsetcnt *)j;
    
    return (p->cnt - o->cnt);
}
