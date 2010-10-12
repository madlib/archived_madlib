/*
 * Cormode-Muthukrishnan CountMin sketch
 * implemented as a user-defined aggregate.
 */
/*
 * The basic CountMin sketch is a set of DEPTH arrays, each with NUMCOUNTERS counters.
 * The idea is that each of those arrays is used as an independent random trial of the
 * same process: each holds counts h_i(x) from a different random hash function h_i.
 * Estimates of the count of some value x are based on the minimum counter h_i(x) across
 * the DEPTH arrays (hence the name CountMin.)
 * 
 * Let's call the process described above "sketching" the x's.
 * We're going to repeat this process LONGBITS times; this is the "dyadic range" trick
 * mentioned in Cormode/Muthu, which repeats the basic CountMin idea log_2(n) times as follows.
 * Every value x/(2^i) is "sketched"
 * at a different power-of-2 (dyadic) "range" i.  So we sketch x in range 0, then sketch x/2
 * in range 1, then sketch x/4 in range 2, etc.
 * 
 * This allows us to count up ranges (like 14-48) by doing CountMin lookups up constituent
 * dyadic ranges (like {[14-15], [16-31], [32-47], [48-48]}).  Dyadic ranges are also useful
 * for histogramming, frequent values, etc.
 */

/*
 * See http://dimacs.rutgers.edu/~graham/pubs/papers/cmencyc.pdf
 * for further explanation
 */

#include "postgres.h"
#include "utils/array.h"
#include "utils/elog.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/numeric.h"
#include "nodes/execnodes.h"
#include "fmgr.h"
#include "sketch_support.h"
#include "catalog/pg_type.h"

#include <ctype.h>

#define LONGBITS (sizeof(int64)*CHAR_BIT)
#define RANGES LONGBITS
#define DEPTH 8
/* #define NUMCOUNTERS 65535 */
#define NUMCOUNTERS 1024

#ifndef MIN
#define MAX(x,y) ((x > y) ? x : y)
#define MIN(x,y) ((x < y) ? x : y)
#define ABS(a)   (((a) < 0) ? -(a) : (a))
#endif

#define MAXVAL (LONG_MAX >> 1)
/* Midpoint is 1/2 of MAX .. i.e. shift MAX right. */
#define MIDVAL (MAXVAL >> 1)
#define MINVAL (LONG_MIN >> 1)


/*
 * the transition value struct for the aggregate.  Holds the sketch counters
 * and a cache of handy metadata that we'll reuse across calls
 */
typedef struct {
    Oid outFuncOid; /* oid of the OutFunc for the data type we're supporting (int8) */
    int64 counters[RANGES][DEPTH][NUMCOUNTERS];
} cmtransval;
#define TRANSVAL_SZ (VARHDRSZ + sizeof(cmtransval))

/*
 * a data structure to hold the constituent power-of-two ranges corresponding to
 * an arbitrary range.  E.g. 14-48 becomes [[14-15], [16-31], [32-47], [48-48]]
 */
typedef struct {
    int64 spans[LONGBITS][2]; /* the ranges */
    int emptyoffset;        /* offset of next empty span */
} rangelist;


void countmin_trans_c(int64 *, char * /* , int */);
bytea *cmsketch_check_transval(bytea *);
void countmin_dyadic_trans_c(int64 *, int64, cmtransval *);
int64 cmsketch_getcount_c(Oid funcOid, int64 *counters, Datum arg);
Datum cmsketch_rangecount_c(cmtransval *, Datum, Datum);
Datum cmsketch_centile_c(cmtransval *, int, int64);
Datum cmsketch_histogram_c(cmtransval *, int64, int64, int);
void find_ranges(int64, int64, rangelist *);
void find_ranges_internal(int64, int64, int, rangelist *);
Datum countmin_dump_c(int64 *);
int64 increment_counter(unsigned int, unsigned int, int64 *, int64);
int64 min_counter(unsigned int, unsigned int, int64 *, int64);
int64 hash_counters_iterate(Datum, int64 *, int64, int64 (*lambdaptr)(
                                unsigned int,
                                unsigned int,
                                int64 *,
                                int64));
bool gt(int64, int64);
bool eq(int64, int64);
bool false_fn(int64, int64);
int64 cmsketch_count_search(cmtransval *, bool (*) (int64, int64), int64, bool (
                                *) (int64, int64), int64);

Datum cmsketch_trans(PG_FUNCTION_ARGS);
Datum cmsketch_getcount(PG_FUNCTION_ARGS);
Datum cmsketch_rangecount(PG_FUNCTION_ARGS);
Datum cmsketch_centile(PG_FUNCTION_ARGS);
Datum cmsketch_histogram(PG_FUNCTION_ARGS);
Datum cmsketch_out(PG_FUNCTION_ARGS);
Datum cmsketch_combine(PG_FUNCTION_ARGS);
Datum cmsketch_dump(PG_FUNCTION_ARGS);


PG_FUNCTION_INFO_V1(cmsketch_trans);

/*
 * This is the UDF interface.  It does sanity checks and preps values
 * for the interesting logic in countmin_dyadic_trans_c
 */
Datum cmsketch_trans(PG_FUNCTION_ARGS)
{
    bytea *     transblob = NULL;
    cmtransval *transval;
    int64 *     counters;
    char *      string;
    /*
     * char           typcategory;
     * bool           typispreferred;
     */

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

    transblob = cmsketch_check_transval(PG_GETARG_BYTEA_P(0));
    transval = (cmtransval *)VARDATA(transblob);

    counters = (int64 *)transval->counters;

    /* get the provided element, being careful in case it's NULL */
    if (!PG_ARGISNULL(1)) {
        /* convert input to a cstring */
        string = OidOutputFunctionCall(transval->outFuncOid,
                                       PG_GETARG_INT64(1));
        countmin_dyadic_trans_c(counters, PG_GETARG_INT64(1), transval);
        PG_RETURN_DATUM(PointerGetDatum(transblob));
    }
    else PG_RETURN_DATUM(PointerGetDatum(transblob));
}

/*
 * if the transblob is not initialized, do so now
 */
bytea *cmsketch_check_transval(bytea *transblob)
{
    bool        typIsVarlena;
    cmtransval *transval;

    /*
     * an uninitialized transval should be a datum smaller than sizeof(cmtransval).
     * if this one is small, initialize it now, else return it.
     */
    if (VARSIZE(transblob) < TRANSVAL_SZ) {
        /* XXX would be nice to pfree the existing transblob, but pfree complains. */

        /* allocate and zero out a transval via palloc0 */
        transblob = (bytea *)palloc0(TRANSVAL_SZ);
        SET_VARSIZE(transblob, TRANSVAL_SZ);

        /* set up outfunc for stringifying INT8's according to PG type rules */
        transval = (cmtransval *)VARDATA(transblob);
        getTypeOutputInfo(INT8OID,
                          &(transval->outFuncOid),
                          &typIsVarlena);
    }
    return(transblob);
}

/*
 * perform multiple sketch insertions, one for each dyadic range (from 0 up to RANGES-1).
 */
void countmin_dyadic_trans_c(int64 *counters,
                             int64 inputi,
                             cmtransval *transval)
{
    char *newstring;
    int   j;

    /*
     *
     */
    for (j = 0; j < RANGES; j++) {
        /* stringify input for the md5 function */
        newstring = OidOutputFunctionCall(transval->outFuncOid, inputi);
        countmin_trans_c(&counters[j*DEPTH*NUMCOUNTERS], newstring /* , (inputi > 0) */);
        /* now divide by 2 for the next dyadic range */
        inputi >>= 1;
    }
}

/*
 * Main loop of Cormode and Muthukrishnan's sketching algorithm, for setting counters in
 * sketches at a single "dyadic range". For each call, we want to use DEPTH independent
 * hash functions.  We do this by using a single md5 hash function, and taking
 * successive 16-bit runs of the result as independent hash outputs.
 */
void countmin_trans_c(int64 *counters, char *input /* , int debug */)
{
    Datum nhash;

    /* get the md5 hash of the input. */
    nhash = md5_datum(input);

    /*
     * iterate through all sketches, incrementing the counters indicated by the hash
     * we don't care about return value here, so 3rd (initialization) argument is arbitrary.
     */
    (void)hash_counters_iterate(nhash, counters, 0, &increment_counter);
}


/*
 * simply returns its input
 * for use as a finalizer in an agg returning the whole sketch
 */
PG_FUNCTION_INFO_V1(cmsketch_out);
Datum cmsketch_out(PG_FUNCTION_ARGS)
{
    PG_RETURN_BYTEA_P(PG_GETARG_BYTEA_P(0));
}

PG_FUNCTION_INFO_V1(cmsketch_combine);
/* UDF wrapper.  All the interesting stuff is in fmsketch_getcount_c */
Datum cmsketch_combine(PG_FUNCTION_ARGS)
{
    bytea *counterblob1 = cmsketch_check_transval((bytea *)PG_GETARG_BYTEA_P(0));
    bytea *counterblob2 = cmsketch_check_transval((bytea *)PG_GETARG_BYTEA_P(1));
    int64 *counters2 = (int64 *)VARDATA(counterblob2);
    bytea *newblob;
    int64 *newcounters;
    int    i;
	int    sz = VARSIZE(counterblob1);

	/* allocate a new transval as a copy of counterblob1 */
    newblob = (bytea *)palloc(sz);
	memcpy(newblob, counterblob1, sz);
    newcounters = (int64 *)VARDATA(newblob);

	/* add in values from counterblob2 */
    for (i = 0;
         i < RANGES*DEPTH*NUMCOUNTERS;
         i++)
        newcounters[i] += counters2[i];

    PG_RETURN_DATUM(PointerGetDatum(newblob));
}


/****** SCALAR FUNCTIONS ON COUNTMIN SKETCHES *******/
PG_FUNCTION_INFO_V1(cmsketch_getcount);

/* scalar function, takes a sketch and a value, produces approximate count of that value * / */
Datum cmsketch_getcount(PG_FUNCTION_ARGS)
{
    bytea *     transblob = cmsketch_check_transval(PG_GETARG_BYTEA_P(0));
    cmtransval *transval = (cmtransval *)VARDATA(transblob);

    /* get the provided element, being careful in case it's NULL */
    if (!PG_ARGISNULL(1)) {
        PG_RETURN_INT64(cmsketch_getcount_c(transval->outFuncOid,
                                            (int64 *)transval->counters,
                                            PG_GETARG_INT64(1)));
    }
    else
        PG_RETURN_NULL();
}

int64 cmsketch_getcount_c(Oid funcOid, int64 *counters, Datum arg)
{
    Datum nhash;

    /* get the md5 hash of the stringified argument. */
    nhash = md5_datum(OidOutputFunctionCall(funcOid, arg));

    /* iterate through the sketches, finding the min counter associated with this hash */
    PG_RETURN_INT64(hash_counters_iterate(nhash, counters, LONG_MAX,
                                          &min_counter));
}

PG_FUNCTION_INFO_V1(cmsketch_rangecount);
/*
 * scalar function, takes a sketch, a min and a max, and produces count of that
 * [min-max] range.
 * This is the UDF wrapper.  All the interesting stuff is in fmsketch_rangecount_c
 */
Datum cmsketch_rangecount(PG_FUNCTION_ARGS)
{
    bytea *     transblob = cmsketch_check_transval(PG_GETARG_BYTEA_P(0));
    cmtransval *transval = (cmtransval *)VARDATA(transblob);
    Oid         element_type1 = get_fn_expr_argtype(fcinfo->flinfo, 1);
    Oid         element_type2 = get_fn_expr_argtype(fcinfo->flinfo, 2);

    if (element_type1 != INT8OID || element_type2 != INT8OID) {
        /* XXX Clean up below to pull string of type names */
        elog(
            ERROR,
            "sketch computed over int8 type; boundaries are %Ld and %Ld.  consider casting.",
            (long long)element_type1,
            (long long)element_type2);
        PG_RETURN_NULL();
    }

    return cmsketch_rangecount_c(transval, PG_GETARG_DATUM(1),
                                 PG_GETARG_DATUM(2));
}

/* Compute count of a range by summing counts of its dyadic ranges */
Datum cmsketch_rangecount_c(cmtransval *transval, Datum bot, Datum top)
{
    int64     cursum = 0;
    int       i;
    rangelist r;
    int64     dyad;
    int64     val;

    r.emptyoffset = 0;
    find_ranges(bot, top, &r);
    for (i = 0; i < r.emptyoffset; i++) {
        /* What power of 2 is this range? */
        dyad = log2(r.spans[i][1] - r.spans[i][0] + 1);
        /* Divide min of range by 2^dyad and get count */
        val = cmsketch_getcount_c(transval->outFuncOid,
                                  &(transval->counters[dyad][0][0]),
                                  (Datum) r.spans[i][0] >> dyad);
        cursum += val;
    }
    PG_RETURN_DATUM(cursum);
}


/*
 * convert an arbitrary range [bot-top] into a rangelist of dyadic ranges.
 * E.g. convert 14-48 into [[14-15], [16-31], [32-47], [48-48]]
 */
void find_ranges(int64 bot, int64 top, rangelist *r)
{
    /* kick off the recursion at power RANGES-1 */
    find_ranges_internal(bot, top, RANGES-1, r);
}

/*
 * find the ranges via recursive calls to this routine, pulling out smaller and
 * smaller powers of 2
 */
void find_ranges_internal(int64 bot, int64 top, int power, rangelist *r)
{
    short  dyad;  /* a number between 0 and 63 */
    uint64 width;

    if (top < bot || power < 0)
        return;

    if (top == bot) {
        /* base case of recursion, a range of the form [x-x]. */
        r->spans[r->emptyoffset][0] = r->spans[r->emptyoffset][1] = bot;
        r->emptyoffset++;
        return;
    }

    /*
     * full MIN-MAX range will overflow int64 variable "width" in the
     * code below, so treat by hand
     */
    if (top == MAXVAL && bot == MINVAL) {
        find_ranges_internal(MINVAL, -1, power-1, r);
        find_ranges_internal(0, MAXVAL, power-1, r);
        return;
    }

    /*
     * if we get here, we have a range of size 2 or greater.
     * Find the largest dyadic range width in this range.
     */
    dyad = trunc(log2(top - bot + 1));
    width = (uint64) pow(2, dyad);

    if ((bot % width) == 0) {
        /* our range is left-aligned on the dyad's min */
        r->spans[r->emptyoffset][0] = bot;
        r->spans[r->emptyoffset][1] = bot + width - 1;
        r->emptyoffset++;
        /* recurse on right at finer grain */
        find_ranges_internal(bot + width, top, power-1, r);
    }

    else if (top == MAXVAL || ((top+1) % width) == 0) {
        /*
         * our range is right-aligned on the dyad's max.
         * the +1 accounts for 0-indexing.
         */
        r->spans[r->emptyoffset][0] = (top - width + 1);
        r->spans[r->emptyoffset][1] = top;
        r->emptyoffset++;
        /* recurse on left at finer grain */
        find_ranges_internal(bot, top - width, power-1, r);
    }
    else {
        /* we straddle a power of 2 */
        int64 power_of_2 = width*(top/width);
        /* recurse on right at finer grain */
        find_ranges_internal(bot, power_of_2 - 1, power-1, r);
        /* recurse on left at finer grain */
        find_ranges_internal(power_of_2, top, power-1, r);
    }
}

PG_FUNCTION_INFO_V1(cmsketch_centile);
/*
 * Scalar function taking a sketch and centile, produces approximate value for
 * that centile.
 * This is the UDF wrapper.  All the interesting stuff is in cmsketch_centile_c
 */
Datum cmsketch_centile(PG_FUNCTION_ARGS)
{
    bytea *     transblob = cmsketch_check_transval(PG_GETARG_BYTEA_P(0));
    cmtransval *transval = (cmtransval *)VARDATA(transblob);
    int         centile = PG_GETARG_INT32(1);
    int64       total;

    total = cmsketch_rangecount_c(transval, MINVAL, MAXVAL);     /* count(*) */
    if (total == 0) PG_RETURN_NULL();

    return cmsketch_centile_c(transval, centile, total);
}

/* find the centile by binary search in the domain of values */
Datum cmsketch_centile_c(cmtransval *transval, int intcentile, int64 total)
{
    int   i;
    int64 higuess,loguess,curguess,curcount;
    int64 centile_cnt;


    if (intcentile <= 0 || intcentile >= 100)
        elog(ERROR, "centiles must be between 1-99 inclusive");


    centile_cnt = (int64)(total * (float8)intcentile/100.0);

    for (i = 0, loguess = MINVAL, higuess = MAXVAL, curguess = 0;
         (i < LONGBITS - 1) && (higuess-loguess > 1);
         i++) {
        curcount = cmsketch_rangecount_c(transval, MINVAL, curguess);
        if (curcount == centile_cnt)
            break;
        if (curcount > centile_cnt) {
            /* overshot */
            higuess = curguess;
            curguess = loguess + (curguess - loguess) / 2;
        }
        else {
            /* undershot */
            loguess = curguess;
            curguess = higuess - (higuess - curguess) / 2;
        }
    }
    return(curguess);
}



PG_FUNCTION_INFO_V1(cmsketch_histogram);
/*
 * scalar function taking a sketch, min, max, and number of buckets.
 * produces an equi-width histogram of that many buckets
 * XXX NOTE would be easy to do an equi-depth histogram: just a bunch of centiles
 */
/* UDF wrapper.  All the interesting stuff is in fmsketch_getcount_c */
Datum cmsketch_histogram(PG_FUNCTION_ARGS)
{
    bytea *     transblob = cmsketch_check_transval(PG_GETARG_BYTEA_P(0));
    cmtransval *transval = (cmtransval *)VARDATA(transblob);
    int         translen = VARSIZE(PG_GETARG_BYTEA_P(0));
    int64       min = PG_GETARG_INT64(1);
    int64       max = PG_GETARG_INT64(2);
    int         buckets = PG_GETARG_INT32(3);
    Datum       retval;

    if (translen == sizeof(cmtransval) + DEPTH*NUMCOUNTERS*sizeof(int64) +
        VARHDRSZ) {
        elog(WARNING, "Cannot compute histogram for a non-integer type");
        PG_RETURN_NULL();
    }

    retval = cmsketch_histogram_c(transval, min, max, buckets);
    PG_RETURN_DATUM(retval);
}

Datum cmsketch_histogram_c(cmtransval *transval,
                           int64 min,
                           int64 max,
                           int buckets)
{
    int64      step;
    int        i;
    ArrayType *retval;
    int64      binlo, binhi, binval;
    Datum      histo[buckets][3];
    int        dims[2], lbs[2];

    step = MAX(trunc((float8)(max-min+1) / (float8)buckets), 1);
    for (i = 0; i < buckets; i++) {
        binlo = min + i*step;
        if (binlo > max) break;
        histo[i][0] = Int64GetDatum(binlo);
        binhi = (i == buckets-1) ? max : (min + (i+1)*step - 1);
        histo[i][1] = Int64GetDatum(binhi);
        binval =
            cmsketch_rangecount_c(transval,
                                  histo[i][0],
                                  histo[i][1]);
        histo[i][2] = Int64GetDatum(binval);
    }


    dims[0] = i;     /* may be less than requested buckets if too few values */
    dims[1] = 3;     /* lo, hi, and val */
    lbs[0] = 0;
    lbs[1] = 0;

    retval = construct_md_array((Datum *)histo,
                                NULL,
                                2,
                                dims,
                                lbs,
                                INT8OID,
                                sizeof(int64),
                                true,
                                'd');
    PG_RETURN_ARRAYTYPE_P(retval);
}

/* XXX Missing: most frequent values */


/****** SUPPORT ROUTINES *******/
PG_FUNCTION_INFO_V1(cmsketch_dump);

Datum cmsketch_dump(PG_FUNCTION_ARGS)
{
    bytea *transblob = (bytea *)PG_GETARG_BYTEA_P(0);
    int64 *counters;
    char * newblob = (char *)palloc(10240);
    int    i, j;

    counters = (int64 *)((cmtransval *)VARDATA(transblob))->counters;
    for (i=0, j=0; i < (VARSIZE(transblob) - VARHDRSZ)/sizeof(int64);
         i++) {
        if (counters[i] != 0)
            j += sprintf(&newblob[j], "[%d:" INT64_FORMAT "], ",
						 i, counters[i]);
        if (j > 10000) break;
    }
    newblob[j] = '\0';
    PG_RETURN_NULL();
}


/*
 * for each row of the sketch, use the 16 bits starting at 2^i mod NUMCOUNTERS,
 * and invoke the lambda on those 16 bits (which may destructively modify counters).
 */
int64 hash_counters_iterate(Datum hashval,
                            int64 *counters,
                            int64 initial,
                            int64 (*lambdaptr)(unsigned int,
                                               unsigned int,
                                               int64 *,
                                               int64))
{
    unsigned int   i, col;
    unsigned short twobytes;
    int64          retval = initial;

    /*
     * XXX WARNING: are there problems with unaligned access here?
     * XXX I'm currently using copies rather than casting, which is inefficient,
     * XXX but I was hoping memmove would deal with unaligned access in a portable way.
     */
    for (i = 0; i < DEPTH; i++) {
        memmove((void *)&twobytes,
                (void *)((char *)VARDATA(DatumGetPointer(hashval)) + 2*i), 2);
        /* elog(WARNING, "twobytes at %d is %d", 2*i, twobytes); */
        col = twobytes % NUMCOUNTERS;
        retval = (*lambdaptr)(i, col, counters, retval);
    }
    return retval;
}


/* destructive increment lambda for hash_counters_iterate.  transval and return val not of particular interest here. */
int64 increment_counter(unsigned int i,
                        unsigned int col,
                        int64 *counters,
                        int64 transval)
{
    int64 oldval = counters[i*NUMCOUNTERS + col];
    /* if (debug) elog(WARNING, "counters[%d][%d] = %ld, incrementing", i, col, oldval); */
    if (counters[i*NUMCOUNTERS + col] == (LONG_MAX >> 1))
        elog(ERROR, "maximum count exceeded in sketch");
    counters[i*NUMCOUNTERS + col] = oldval + 1;

    return oldval+1;
}

/* running minimum lambda for hash_counters_iterate */
int64 min_counter(unsigned int i,
                  unsigned int col,
                  int64 *counters,
                  int64 transval)
{
    int64 thisval = counters[i*NUMCOUNTERS + col];
    return (thisval < transval) ? thisval : transval;
}
