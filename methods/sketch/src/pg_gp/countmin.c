/*!
 * \file countmin.c
 *
 * \brief CountMin sketch implementation
 *
 * \implementation
 * The basic CountMin sketch is a set of DEPTH arrays, each with NUMCOUNTERS counters.
 * The idea is that each of those arrays is used as an independent random trial of the
 * same process: for all the values x in a set, each holds counts of h_i(x) mod NUMCOUNTERS for a different random hash function h_i.
 * Estimates of the count of some value x are based on the <i>minimum</i> counter h_i(x) across
 * the DEPTH arrays (hence the name CountMin.)
 *
 * Let's call the process described above "sketching" the x's.  To support range
 * lookups, we repeat the basic CountMin sketching process INT64BITS times as follows.
 * (This is the "dyadic range" trick mentioned in Cormode/Muthu.)
 *
 * Every value x/(2^i) is sketched
 * at a different power-of-2 (dyadic) "range" i.  So we sketch x in range 0, then sketch x/2
 * in range 1, then sketch x/4 in range 2, etc.
 * This allows us to count up ranges (like 14-48) by doing CountMin equality lookups on constituent
 * dyadic ranges ({[14-15] as 7 in range 2, [16-31] as 1 in range 16, [32-47] as 2 in range 16, [48-48] as 48 in range 1}).
 * Dyadic ranges are similarly useful for histogramming, order stats, etc.
 *
 * The results of the estimators below generally have guarantees of the form
 * "the answer is within \epsilon of the true answer with probability 1-\delta."
 */

#include <postgres.h>
#include <utils/fmgroids.h>
#include <utils/array.h>
#include <utils/elog.h>
#include <utils/builtins.h>
#include <utils/lsyscache.h>
#include <utils/numeric.h>
#include <nodes/execnodes.h>
#include <fmgr.h>
#include <catalog/pg_type.h>
#include <ctype.h>
#include "sketch_support.h"
#include "countmin.h"

PG_FUNCTION_INFO_V1(__cmsketch_int8_trans);

/*
 * This is the UDF interface.  It does sanity checks and preps values
 * for the interesting logic in countmin_dyadic_trans_c
 */
Datum __cmsketch_int8_trans(PG_FUNCTION_ARGS)
{
    bytea *     transblob = NULL;
    cmtransval *transval;

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

    /* get the provided element, being careful in case it's NULL */
    if (!PG_ARGISNULL(1)) {
        transblob = cmsketch_check_transval(fcinfo, true);
        transval = (cmtransval *)(VARDATA(transblob));

        /* the following line modifies the contents of transval, and hence transblob */
        countmin_dyadic_trans_c(transval, PG_GETARG_DATUM(1));
        PG_RETURN_DATUM(PointerGetDatum(transblob));
    }
    else PG_RETURN_DATUM(PointerGetDatum(PG_GETARG_BYTEA_P(0)));
}

/*!
 * check if the transblob is not initialized, and do so if not
 * \param transblob a cmsketch transval packed in a bytea
 */
bytea *cmsketch_check_transval(PG_FUNCTION_ARGS, bool initargs)
{
    bytea *     transblob = PG_GETARG_BYTEA_P(0);
    cmtransval *transval;

    /*
     * an uninitialized transval should be a datum smaller than sizeof(cmtransval).
     * if this one is small, initialize it now, else return it.
     */
    if (!CM_TRANSVAL_INITIALIZED(transblob)) {
        /* XXX would be nice to pfree the existing transblob, but pfree complains. */
        transblob = cmsketch_init_transval();
        transval = (cmtransval *)VARDATA(transblob);

        if (initargs) {
            int nargs = PG_NARGS();
            int i;
            /* carry along any additional args */
            if (nargs - 2 > MAXARGS)
                elog(
                    ERROR,
                    "no more than %d additional arguments should be passed to __cmsketch_int8_trans",
                    MAXARGS);
            transval->nargs = nargs - 2;
            for (i = 2; i < nargs; i++) {
                if (PG_ARGISNULL(i))
                    elog(ERROR,
                         "NULL parameter %d passed to __cmsketch_int8_trans",
                         i);
                transval->args[i-2] = PG_GETARG_INT64(i);
            }
        }
        else transval->nargs = -1;
    }
    return(transblob);
}

bytea *cmsketch_init_transval()
{
    /* allocate and zero out a transval via palloc0 */
    bytea *     transblob = (bytea *)palloc0(CM_TRANSVAL_SZ);
    SET_VARSIZE(transblob, CM_TRANSVAL_SZ);

    return(transblob);
}

/*!
 * perform multiple sketch insertions, one for each dyadic range (from 0 up to RANGES-1).
 * * \param transval the cmsketch transval
 * * \param inputi the value to be inserted
 */
void countmin_dyadic_trans_c(cmtransval *transval, Datum input)
{
    uint32 j;

    for (j = 0; j < RANGES; j++) {
        countmin_trans_c(transval->sketches[j], input,
                         F_INT8OUT, INT8OID);
        /* now divide by 2 for the next dyadic range */
        input = Int64GetDatum(DatumGetInt64(input) >> 1);
    }
}

/*!
 * Main loop of Cormode and Muthukrishnan's sketching algorithm, for setting counters in
 * sketches at a single "dyadic range". For each call, we want to use DEPTH independent
 * hash functions.  We do this by using a single md5 hash function, and taking
 * successive 16-bit runs of the result as independent hash outputs.
 * \param sketch the current countmin sketch
 * \param dat the datum to be inserted
 * \param outFuncOid Oid of the PostgreSQL function to convert dat to a string
 * \param typOid Oid of the Postgres type for dat
 */
Datum countmin_trans_c(countmin sketch, Datum dat, Oid outFuncOid, Oid typOid)
{
	(void) outFuncOid; /* avoid warning about unused parameter */
    bytea *nhash;

    nhash = sketch_md5_bytea(dat, typOid);

    /*
     * iterate through all sketches, incrementing the counters indicated by the hash
     * we don't care about return value here, so 3rd (initialization) argument is arbitrary.
     */
    (void)hash_counters_iterate(nhash, sketch, 0, &increment_counter);
    return(PointerGetDatum(nhash));
}

/*
 * FINAL functions for various UDAs built on countmin sketches
 */

/*
 * pasted code from utils/encode.c, facilitating __cmsketch_base64_final
 */
static const char _base64[] =
"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const int8 b64lookup[128] = {
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 62, -1, -1, -1, 63,
    52, 53, 54, 55, 56, 57, 58, 59, 60, 61, -1, -1, -1, -1, -1, -1,
    -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
    15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, -1, -1, -1, -1, -1,
    -1, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
    41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, -1, -1, -1, -1, -1,
};

static unsigned
b64_encode(const char *src, unsigned len, char *dst)
{
    char       *p,
               *lend = dst + 76;
    const char *s,
               *end = src + len;
    int         pos = 2;
    uint32      buf = 0;

    s = src;
    p = dst;

    while (s < end)
    {
        buf |= (unsigned char) *s << (pos << 3);
        pos--;
        s++;

        /* write it out */
        if (pos < 0)
        {
            *p++ = _base64[(buf >> 18) & 0x3f];
            *p++ = _base64[(buf >> 12) & 0x3f];
            *p++ = _base64[(buf >> 6) & 0x3f];
            *p++ = _base64[buf & 0x3f];

            pos = 2;
            buf = 0;
        }
        if (p >= lend)
        {
            *p++ = '\n';
            lend = p + 76;
        }
    }
    if (pos != 2)
    {
        *p++ = _base64[(buf >> 18) & 0x3f];
        *p++ = _base64[(buf >> 12) & 0x3f];
        *p++ = (pos == 0) ? _base64[(buf >> 6) & 0x3f] : '=';
        *p++ = '=';
    }

    return p - dst;
}

static unsigned
b64_enc_len(unsigned srclen)
{
    /* 3 bytes will be converted to 4, linefeed after 76 chars */
    return (srclen + 2) * 4 / 3 + srclen / (76 * 3 / 4);
}
// done pasted code

/*!
 * return the array of sketch counters as a int8
 */
PG_FUNCTION_INFO_V1(__cmsketch_base64_final);
Datum __cmsketch_base64_final(PG_FUNCTION_ARGS)
{
    bytea *     blob = PG_GETARG_BYTEA_P(0);
    cmtransval *sketch = NULL;
    int         len = RANGES*sizeof(countmin) + VARHDRSZ;
    bytea *out = NULL;

    if (VARSIZE(blob) > VARHDRSZ && !CM_TRANSVAL_INITIALIZED(blob)) {
        elog(ERROR, "invalid transition state for cmsketch");
    }

    out = palloc0(len);
    if (VARSIZE(blob) > VARHDRSZ) {
        sketch = (cmtransval *)VARDATA(blob);
        memcpy((uint8 *)VARDATA(out), sketch->sketches, len - VARHDRSZ);
    }
    SET_VARSIZE(out, len);

    // pasted code from utils/builtins.h:binary_encode()
    bytea *data = out;
    text       *result;
    int         datalen,
                resultlen,
                res;
    datalen = VARSIZE(data) - VARHDRSZ;
    resultlen = b64_enc_len(datalen);
    result = palloc(VARHDRSZ + resultlen);
    res = b64_encode(VARDATA(data), datalen, VARDATA(result));
    /* Make this FATAL 'cause we've trodden on memory ... */
    if (res > resultlen)
        elog(FATAL, "overflow - encode estimate too small");
    SET_VARSIZE(result, VARHDRSZ + res);

    PG_RETURN_TEXT_P(result);
}

/*!
 * Greenplum "prefunc" to combine sketches from multiple machines
 */
PG_FUNCTION_INFO_V1(__cmsketch_merge);
Datum __cmsketch_merge(PG_FUNCTION_ARGS)
{
    bytea *     counterblob1 = PG_GETARG_BYTEA_P(0);
    bytea *     counterblob2 = PG_GETARG_BYTEA_P(1);
    cmtransval *transval2 = (cmtransval *)VARDATA(counterblob2);
    cmtransval *newtrans;
    countmin *  sketches2 = NULL;
    bytea *     newblob;
    countmin *  newsketches;
    uint32      i, j, k;
    int         sz;

    /* make sure they're initialized! */
    if (!CM_TRANSVAL_INITIALIZED(counterblob1)
        && !CM_TRANSVAL_INITIALIZED(counterblob2))
        /* if both are empty can return one of them */
        PG_RETURN_DATUM(PointerGetDatum(counterblob1));
    else if (!CM_TRANSVAL_INITIALIZED(counterblob1)) {
        counterblob1 = cmsketch_init_transval();
    }
    else if (!CM_TRANSVAL_INITIALIZED(counterblob2)) {
        counterblob2 = cmsketch_init_transval();
        transval2 = (cmtransval *)VARDATA(counterblob2);
    }

    sketches2 = transval2 ->sketches;

    sz = VARSIZE(counterblob1);
    /* allocate a new transval as a copy of counterblob1 */
    newblob = (bytea *)palloc(sz);
    memcpy(newblob, counterblob1, sz);
    newtrans = (cmtransval *)(VARDATA(newblob));
    newsketches = (countmin *)(newtrans)->sketches;

    /* add in values from counterblob2 */
    for (i = 0; i < RANGES; i++)
        for (j = 0; j < DEPTH; j++)
            for (k = 0; k < NUMCOUNTERS; k++)
                newsketches[i][j][k] += sketches2[i][j][k];

    if (newtrans->nargs == -1) {
        /* transfer in the args from the other input */
        newtrans->nargs = transval2->nargs;
        for (i = 0; (int)i < transval2->nargs; i++)
            newtrans->args[i] = transval2->args[i];
    }

    PG_RETURN_DATUM(PointerGetDatum(newblob));
}



/*
 *******  Below are scalar methods to manipulate completed sketches.  ******
 */

/*!
 * get the approximate count of objects with value arg
 * \param sketch a countmin sketch
 * \param arg the Datum we want to find the count of
 * \param funcOid the Postgres function that converts arg to a string
 */
int64 cmsketch_count_c(countmin sketch, Datum arg, Oid funcOid, Oid typOid)
{
    bytea *nhash;

    /* get the md5 hash of the argument. */
    nhash = sketch_md5_bytea(arg, typOid);
    return(cmsketch_count_md5_datum(sketch, nhash, funcOid));
}

int64 cmsketch_count_md5_datum(countmin sketch, bytea *md5_bytea, Oid funcOid)
{
	(void) funcOid; /* avoid warning about unused parameter */
    /* iterate through the sketches, finding the min counter associated with this hash */
    return(hash_counters_iterate(md5_bytea, sketch, INT64_MAX,
                                          &min_counter));
}


#if 0
/****** SUPPORT ROUTINES *******/
PG_FUNCTION_INFO_V1(cmsketch_dump);

/*!
 * dump sketch contents
 */
Datum cmsketch_dump(PG_FUNCTION_ARGS)
{
    bytea *   transblob = (bytea *)PG_GETARG_BYTEA_P(0);
    countmin *sketches;
    char *    newblob = (char *)palloc(10240);
    uint32    i, j, k, c;

    if (VARSIZE(transblob) > VARHDRSZ && !CM_TRANSVAL_INITIALIZED(transblob)) {
        elog(ERROR, "invalid transition state for cmsketch");
    }

    if (VARSIZE(transblob) > VARHDRSZ) {
        sketches = ((cmtransval *)VARDATA(transblob))->sketches;
    }
    else {
        sketches = palloc0(RANGES*sizeof(countmin) + VARHDRSZ);
        SET_VARSIZE(sketches, RANGES*sizeof(countmin) + VARHDRSZ);
    }
    for (i=0, c=0; i < RANGES; i++)
        for (j=0; j < DEPTH; j++)
            for(k=0; k < NUMCOUNTERS; k++) {
                if (sketches[i][j][k] != 0)
                    c += sprintf(&newblob[c], "[(%d,%d,%d):" INT64_FORMAT
                                 "], ", i, j, k, sketches[i][j][k]);
                if (c > 10000) break;
            }
    newblob[c] = '\0';
    if (VARSIZE(transblob) <= VARHDRSZ) {
        pfree(sketches);
    }
    PG_RETURN_NULL();
}
#endif


/*!
 * for each row of the sketch, use the 16 bits starting at 2^i mod NUMCOUNTERS,
 * and invoke the lambda on those 16 bits (which may destructively modify counters).
 * \param hashval the MD5 hashed value that we take 16 bits at a time
 * \param sketch the cmsketch
 * \param initial the initialized return value
 * \param lambdaptr the function to invoke on each 16 bits
 */
int64 hash_counters_iterate(bytea *hashval,
                            countmin sketch, /* width is DEPTH*NUMCOUNTERS */
                            int64 initial,
                            int64 (*lambdaptr)(uint32,
                                               uint32,
                                               countmin,
                                               int64))
{
    uint32         i, col;
    char          *c;
    unsigned short twobytes;
    int64          retval = initial;

    /*
     * XXX WARNING: are there problems with unaligned access here?
     * XXX I was using memmove rather than casting, which was inefficient,
     * XXX but I was hoping memmove would deal with unaligned access in a portable way.
     * XXX However the deref of 2 bytes seems to work OK.
     */
    for (i = 0, c = (char *)VARDATA(hashval);
         i < DEPTH;
         i++, c += 2) {
        twobytes = *(unsigned short *)c;
        col = twobytes % NUMCOUNTERS;
        retval = (*lambdaptr)(i, col, sketch, retval);
    }
    return retval;
}

/*!
 * destructive increment lambda for hash_counters_iterate.
 * transval and return val not of particular interest here.
 * \param i which row to update
 * \param col which column to update
 * \param sketch the sketch
 * \param transval we don't need transval here, but its part of the
 * lambda interface for hash_counters_iterate
 */

int64 increment_counter(uint32 i,
                        uint32 col,
                        countmin sketch,
                        int64 transval)
{
	(void) transval; /* avoid warning about unused parameter */
    int64 oldval = sketch[i][col];
    if (sketch[i][col] == (INT64_MAX))
        elog(ERROR, "maximum count exceeded in sketch");
    sketch[i][col] = oldval + 1;

    /* return the incremented value, though unlikely anyone cares. */
    return oldval+1;
}

/*!
 * running minimum lambda for hash_counters_iterate
 * \param i which row to examine
 * \param col which column to examine
 * \param sketch the sketch
 * \param transval smallest counter so far
 * lambda interface for hash_counters_iterate
 */
int64 min_counter(uint32 i,
                  uint32 col,
                  countmin sketch,
                  int64 transval)
{
    int64 thisval = sketch[i][col];
    return (thisval < transval) ? thisval : transval;
}
