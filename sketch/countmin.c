// Cormode-Muthukrishnan CountMin sketch
// implemented as a user-defined aggregate.  
// See http://dimacs.rutgers.edu/~graham/pubs/papers/cmencyc.pdf
// for explanation 

/* THIS CODE MAY NEED TO BE REVISITED TO ENSURE ALIGNMENT! */

#include "postgres.h"
#include "utils/array.h"
#include "utils/elog.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "utils/numeric.h"
#include "libpq/md5.h"
#include "nodes/execnodes.h"
#include "fmgr.h"
#include "sketch_support.h"
#include "catalog/pg_type.h"

#include <ctype.h>

#define NUMSKETCHES 64
#define ROWS 8
/* #define COLUMNS 65535 */
#define COLUMNS 1024

#ifndef MIN
#define MAX(x,y) ((x > y) ? x : y)
#define MIN(x,y) ((x < y) ? x : y)
#define ABS(a)   (((a) < 0) ? -(a) : (a))
#endif

typedef struct {
  int64 next;
  Datum spans[62][2];
} rangelist;

typedef struct {
  Oid outFuncOid;
  Oid elementTypeOid;
  int64 counters[1];
} cmtransval;

void countmin_trans_c(int64 *, char * /* , int */);
void countmin_num_trans_c(int64 *, Datum, cmtransval *);
Datum countmin_getcount_c(int64 *, char *input);
Datum cmsketch_rangecount_c(cmtransval *, Datum, Datum);
Datum cmsketch_centile_c(cmtransval *transval, int centile);
Datum cmsketch_histogram_c(cmtransval *transval, int buckets);
int64 cmsketch_min(int64 *counters, char *hashbits);
void find_ranges(Datum, Datum, int, rangelist *, Oid);
Datum countmin_dump_c(int64 *);

Datum cmsketch_trans(PG_FUNCTION_ARGS);
Datum cmsketch_getcount(PG_FUNCTION_ARGS);
Datum cmsketch_rangecount(PG_FUNCTION_ARGS);
Datum cmsketch_centile(PG_FUNCTION_ARGS);
Datum cmsketch_histogram(PG_FUNCTION_ARGS);
Datum cmsketch_out(PG_FUNCTION_ARGS);
Datum cmsketch_combine(PG_FUNCTION_ARGS);
Datum cmsketch_dump(PG_FUNCTION_ARGS);

#define DatumGetValTypeOid(d) DatumGetVal((d), typeOid)
#define DatumGetVal(d, typeOid) \
    ((typeOid == INT2OID) ? DatumGetInt16(d) : \
      ((typeOid == INT4OID) ? DatumGetInt32(d) : \
        ((typeOid == INT8OID) ? DatumGetInt64(d) : \
              0)))

#define ValGetDatumTypeOid(val) ValGetDatum((val), typeOid)
#define ValGetDatum(val, typeOid) \
    ((typeOid == INT2OID) ? Int16GetDatum(val) : \
      ((typeOid == INT4OID) ? Int32GetDatum(val) : \
        ((typeOid == INT8OID) ? Int64GetDatum(val) : \
              0)))

#define TYPLEN(typeOid) \
               ((typeOid == INT2OID) ? 16 : \
                  ((typeOid == INT4OID) ? 32 : \
                    ((typeOid == INT8OID) ? 64 : \
                          0)))
                          
#define MAXVAL(typeOid) \
               ((typeOid == INT2OID) ? SHRT_MAX >> 1 : \
                  ((typeOid == INT4OID) ? INT_MAX >> 1 : \
                    ((typeOid == INT8OID) ? LONG_MAX >> 1: \
                          0)))
#define MIDVAL(typeOid) (MAXVAL(typeOid) >> 1)

#define MINVAL(typeOid) \
               ((typeOid == INT2OID) ? SHRT_MIN >> 1 : \
                  ((typeOid == INT4OID) ? INT_MIN >> 1 : \
                    ((typeOid == INT8OID) ? LONG_MIN >> 1: \
                          0)))
        
        
PG_FUNCTION_INFO_V1(cmsketch_trans);

// This is the UDF interface.  It just converts args into C strings and 
// calls the interesting logic in countmin_trans_c
Datum cmsketch_trans(PG_FUNCTION_ARGS)
{
  bytea          *transblob = NULL;
  cmtransval     *transval;
  int64          *counters;
  Oid            element_type;
  bool           typIsVarlena;
  int            blobsz;
  char           *string;
  // char           typcategory;
  // bool           typispreferred;
  int            numsketches;
	
  transblob = (bytea *)PG_GETARG_BYTEA_P(0);
  
  // SETUP
  // if the transval is at its init value
  if (VARSIZE(transblob) <= VARHDRSZ+8) {
    element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
    if (!OidIsValid(element_type))
        elog(ERROR, "could not determine data type of input");

    // This function makes destructive updates via array_set_bit_in_place.
    // Make sure it's being called in an agg context.
    if (!(fcinfo->context &&
                    (IsA(fcinfo->context, AggState) 
      #ifdef NOTGP
                     || IsA(fcinfo->context, WindowAggState)
      #endif
      	      )))
            elog(NOTICE, "destructive pass by reference outside agg");

    // PG8.4 has typecategories, but Greenplum does not, so we'll explicitly
    // enumerate our supported types
    // get_type_category_preferred(element_type, &typcategory, &typispreferred);
    // if (typecategory = TYPCATEGORY_NUMERIC)
    switch (element_type) {
      case INT2OID:
      case INT4OID:
      case INT8OID:
      // case FLOAT4OID:
      // case FLOAT8OID:
        numsketches = NUMSKETCHES;
        break;
      default:
        numsketches = 1;
    }

    // allocate numsketches*ROWS*COLUMNS*sizeof(int64) to store what we need.  
    // elog(WARNING, "counters size = %u", VARSIZE(PG_GETARG_BYTEA_P(0)));
    // elog(WARNING, "initializing transval");
    blobsz = VARHDRSZ + sizeof(cmtransval) + numsketches*ROWS*COLUMNS*sizeof(int64);
    // elog(NOTICE, "pallocing %d (%d sketches * %d rows * %d columns * 64)", blobsz,
         // numsketches, ROWS, COLUMNS);
    transblob = (bytea *)palloc(blobsz);
    SET_VARSIZE(transblob, blobsz);
    
    // set up outfunc
    transval = (cmtransval *)VARDATA(transblob);
    transval->elementTypeOid = element_type;
    getTypeOutputInfo(element_type, &(transval->outFuncOid), &typIsVarlena);
  }
  else {
    // transval is all set up, just extract it
    transval = (cmtransval *)VARDATA(transblob);
    // elog(WARNING, "fetched transval, size %d", VARSIZE(transblob));
    element_type = transval->elementTypeOid;
  }
  
  counters = transval->counters;
  
  /* get the provided element, being careful in case it's NULL */
  if (!PG_ARGISNULL(1)) {
    // convert input to a cstring
    string = OidOutputFunctionCall(transval->outFuncOid, PG_GETARG_DATUM(1));      
    countmin_trans_c(counters, string /* , 0 */);
    
    // for numeric types we do the "dyadic ranges" as well
    switch (element_type) {
      case INT2OID:
      case INT4OID:
      case INT8OID:
      // case FLOAT4OID:
      // case FLOAT8OID:
        countmin_num_trans_c(counters, PG_GETARG_DATUM(1), transval);
    }
    // elog(WARNING, "returning transval of size %d", VARSIZE(transblob));
    PG_RETURN_DATUM(PointerGetDatum(transblob));
  }
  else PG_RETURN_DATUM(PointerGetDatum(transblob)); 
}

void countmin_num_trans_c(int64 *counters, Datum val, cmtransval *transval)
{
  int64 inputi = (int64)DatumGetVal(val, transval->elementTypeOid);
  char *newstring;
  int j;

  // if (inputi < 0)
  //   elog(ERROR, "CountMin sketches currently cannot handle negative values");

  /* for each power of 2 from 1 to NUMSKETCHES-1 we divide input numbers by another 
      factor of 2 before sketching */
  for (j = 1; j < NUMSKETCHES; j++) {
    inputi >>= 1;
    newstring = OidOutputFunctionCall(transval->outFuncOid, inputi);      
    // elog(WARNING, "inputi = %ld, newstring = \"%s\", sketchno = %d", inputi, newstring, j);
    countmin_trans_c(&counters[ROWS*COLUMNS*j], newstring /* , (inputi > 0) */);
  }
}

// Main loop of Cormode and Muthukrishnan's sketching algorithm.
// For each call, we get an md5 hash of the value passed in.
// We use successive 16-bit runs of the hash to choose a cell in each row of the sketch to increment.
void countmin_trans_c(int64 *counters, char *input /* , int debug */) 
{
//  uint64 index = 0, tmp;
//  unsigned char c[MD5_HASHLEN_BITS/CHAR_BIT+1];
//  int rmost;
  // unsigned int hashes[MD5_HASHLEN_BITS/sizeof(unsigned int)];
  int i;
  // bytea *digest;
  Datum result;
  Datum nhash;
  unsigned int col;
  unsigned short twobytes;
  int64 oldval;
  
  /* get the md5 hash of the input.  Return val is textual representation of a big hex number. */
  result = DirectFunctionCall1(md5_bytea, PointerGetDatum(cstring_to_text(input)) /* CStringGetTextDatum(input) */);
  nhash = DirectFunctionCall2(binary_decode, result, CStringGetTextDatum("hex"));
  // bit_print((unsigned char *)VARDATA(nhash), VARSIZE(nhash) - VARHDRSZ);

  /* for each row of the sketch, use the 16 bits starting at 2^i mod COLUMNS, and
     increment the corresponding cell. */
  for (i = 0; i < ROWS; i++) {
    memmove((void *)&twobytes, (void *)((char *)VARDATA(nhash) + 2*i), 2);
    // elog(WARNING, "twobytes at %d is %d", 2*i, twobytes);
    col = twobytes % COLUMNS;
    oldval = counters[i*COLUMNS + col];  
    // if (debug) elog(WARNING, "counters[%d][%d] = %ld, incrementing", i, col, oldval);
    if (counters[i*COLUMNS + col] == (LONG_MAX >> 1))
      elog(ERROR, "maximum count exceeded in sketch");
    counters[i*COLUMNS + col] = oldval + 1;
  }
}

PG_FUNCTION_INFO_V1(cmsketch_getcount);

// UDF wrapper.  All the interesting stuff is in fmsketch_getcount_c
Datum cmsketch_getcount(PG_FUNCTION_ARGS)
{
  bytea          *transblob = (bytea *)PG_GETARG_BYTEA_P(0);
  int64          *counters;
  Oid            element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
  Oid            funcOid;
  bool           typIsVarlena;
  char           *string;

  counters = ((cmtransval *)VARDATA(transblob))->counters;

  /* get the provided element, being careful in case it's NULL */
  if (!PG_ARGISNULL(1)) {
    // figure out the outfunc for this type
    getTypeOutputInfo(element_type, &funcOid, &typIsVarlena);
    
    // convert input to a cstring
    string = OidOutputFunctionCall(funcOid, PG_GETARG_DATUM(1));

    // elog(WARNING, "returning bytea of size %d", VARSIZE(counters));
    PG_RETURN_DATUM(countmin_getcount_c(counters, string));
  }
  else
    PG_RETURN_NULL();

  return countmin_getcount_c(counters, string);
}

int64 cmsketch_min(int64 *counters, char *hashbits)
{
  int64 min = 0, curmin;
  int i;
  unsigned short twobytes;
  unsigned int col;

  for (i = 0; i < ROWS; i++)
  {
    memmove((void *)&twobytes, (void *)(hashbits + 2*i), 2);
    col = twobytes % COLUMNS;
    curmin = counters[i*COLUMNS + col];
    // elog(WARNING, "counters[%d][%d] = %ld", i, col, curmin);
    if (min == 0 || min > curmin) min = curmin;
  }
  return min;
}
// Compute count of a group by finding the min hash entry across the ROWS.
// Only needs to look at the first sketch.
Datum countmin_getcount_c(int64 *counters, char *input)
{
  // unsigned char c[MD5_HASHLEN_BITS/CHAR_BIT+1];
  // bytea *digest;
  Datum result;
  Datum nhash;
  
  /* get the md5 hash of the input.  Return val is textual representation of a big hex number. */
  result = DirectFunctionCall1(md5_bytea, PointerGetDatum(cstring_to_text(input)));
  // digest = (bytea *)DatumGetPointer(result);
  nhash = DirectFunctionCall2(binary_decode, result, CStringGetTextDatum("hex"));
    
  PG_RETURN_INT64(cmsketch_min(counters, VARDATA(nhash)));
}

// This routine only works on numeric types
void find_ranges(Datum bot, Datum top, int power, rangelist *r, Oid typeOid)
{
  int i;
  int64 raised;
  
  if (DatumGetValTypeOid(top) < DatumGetValTypeOid(bot))
    return;
  
  // elog(WARNING, "find_ranges(%ld, %ld, %d, r, %ld)\n", (int64)DatumGetValTypeOid(bot), (int64)DatumGetValTypeOid(top), power, (int64)typeOid);
  if (DatumGetValTypeOid(top) == DatumGetValTypeOid(bot)) {
    r->spans[r->next][0] = r->spans[r->next][1] = bot;
    // elog(WARNING, "%d: [%ld, %ld]\n", 1, r->spans[r->next][0], r->spans[r->next][1]);
    r->next++;
    return;
  }
  
  i = trunc(log2(DatumGetValTypeOid(top) - DatumGetValTypeOid(bot) + 1));
  raised = (int64) pow(2, i);
  // elog(WARNING, "raised is %ld", raised);

  if ((int64)DatumGetValTypeOid(bot) % raised == 0) {
    r->spans[r->next][0] = bot;
    r->spans[r->next][1] = ValGetDatumTypeOid(DatumGetValTypeOid(bot) + raised - 1);
    // elog(WARNING, "atbot %ld: [%ld, %ld]\n", raised, r->spans[r->next][0], r->spans[r->next][1]);
    r->next++;
    find_ranges(ValGetDatumTypeOid(DatumGetValTypeOid(bot) + (raised)), top, i-1, r, typeOid);
  }
  else if ((int64)(DatumGetValTypeOid(top)+1) % raised == 0) {
    r->spans[r->next][0] = ValGetDatumTypeOid(DatumGetValTypeOid(top) - raised + 1);
    r->spans[r->next][1] = top;
    // elog(WARNING, "attop %ld: [%ld, %ld]\n", raised, r->spans[r->next][0], r->spans[r->next][1]);
    r->next++;
    find_ranges(bot, ValGetDatumTypeOid(DatumGetValTypeOid(top) - raised), i-1, r, typeOid);
  }
  else { //we straddle a boundary
    find_ranges(bot, ValGetDatumTypeOid(raised * (DatumGetValTypeOid(top)/raised)) - 1, i-1, r, typeOid);
    find_ranges(ValGetDatumTypeOid(raised * (DatumGetValTypeOid(top)/raised)), top, i-1, r, typeOid);
  }
}

PG_FUNCTION_INFO_V1(cmsketch_out);
Datum cmsketch_out(PG_FUNCTION_ARGS)
{  
  PG_RETURN_BYTEA_P(PG_GETARG_BYTEA_P(0));
}

PG_FUNCTION_INFO_V1(cmsketch_rangecount);
// UDF wrapper.  All the interesting stuff is in fmsketch_rangecount_c
Datum cmsketch_rangecount(PG_FUNCTION_ARGS)
{
  bytea          *transblob = (bytea *)PG_GETARG_BYTEA_P(0);
  cmtransval     *transval = (cmtransval *)VARDATA(transblob);
  Oid            element_type1 = get_fn_expr_argtype(fcinfo->flinfo, 1);  
  Oid            element_type2 = get_fn_expr_argtype(fcinfo->flinfo, 2);  
  int            translen = VARSIZE(PG_GETARG_BYTEA_P(0));
  // int64          bot = PG_GETARG_INT64(1);
  // int64          top = PG_GETARG_INT64(2);
    
  // elog(NOTICE, "transval is %d long", translen);
  if (element_type1 != transval->elementTypeOid || element_type2 != transval->elementTypeOid) {
    elog(ERROR, "sketch computed over %Ld type; boundaries are %Ld and %Ld.  consider casting.",
         (long long)transval->elementTypeOid, (long long)element_type1, (long long)element_type2);
    PG_RETURN_NULL();
  }
      
  if (translen == sizeof(cmtransval) + ROWS*COLUMNS*sizeof(int64) + VARHDRSZ) {
    elog(WARNING, "Cannot perform rangecount for a non-integer type");
    PG_RETURN_NULL();
  }
  
  return cmsketch_rangecount_c(transval, PG_GETARG_DATUM(1), PG_GETARG_DATUM(2));
}

// Compute count of a group by finding the min hash entry across the ROWS.
Datum cmsketch_rangecount_c(cmtransval *transval, Datum bot, Datum top)
{
  int64 *counters = transval->counters;
  Oid typeOid = transval->elementTypeOid;
  int64 cursum = 0;
  int i;
  // unsigned char c[MD5_HASHLEN_BITS/CHAR_BIT+1];
  // bytea *digest;
  Datum result;
  Datum nhash;
  rangelist r;
  int sketchno;
  char *newstring;
  int64 *cursketch;
  
  r.next = 0;
  find_ranges(bot, top, 63, &r, typeOid);
  for (i = 0; i < r.next; i++) {
    // XXX ARITHMETIC
    sketchno = log2(DatumGetValTypeOid(r.spans[i][1]) - DatumGetValTypeOid(r.spans[i][0]) + 1);
    newstring = OidOutputFunctionCall(transval->outFuncOid, r.spans[i][0] >> sketchno);      
    /* get the md5 hash of the input.  Return val is textual representation of a big hex number. */
    result = DirectFunctionCall1(md5_bytea, PointerGetDatum(cstring_to_text(newstring)));
    // digest = (bytea *)DatumGetPointer(result);
    nhash = DirectFunctionCall2(binary_decode, result, CStringGetTextDatum("hex"));
    // elog(WARNING, "Summing in sketchno %d for string \"%s\"", sketchno, newstring);
    cursketch = &(counters[sketchno*ROWS*COLUMNS]);
    cursum += cmsketch_min(cursketch, VARDATA(nhash));
  }
  PG_RETURN_DATUM(cursum);
}

PG_FUNCTION_INFO_V1(cmsketch_centile);
// UDF wrapper.  All the interesting stuff is in fmsketch_getcount_c
Datum cmsketch_centile(PG_FUNCTION_ARGS)
{
  bytea          *transblob = (bytea *)PG_GETARG_BYTEA_P(0);
  cmtransval     *transval = (cmtransval *)VARDATA(transblob);
  int            translen = VARSIZE(PG_GETARG_BYTEA_P(0));
  int            centile = PG_GETARG_INT32(1);
    
  // elog(NOTICE, "transval is %d long", translen);
  if (translen == sizeof(cmtransval) + ROWS*COLUMNS*sizeof(int64) + VARHDRSZ) {
    elog(WARNING, "Cannot compute centile for a non-integer type");
    PG_RETURN_NULL();
  }
  
  return cmsketch_centile_c(transval, centile);
}

Datum cmsketch_centile_c(cmtransval *transval, int intcentile)
{
  Datum minval = MINVAL(transval->elementTypeOid);
  Datum maxval = MAXVAL(transval->elementTypeOid);
  int64 total = cmsketch_rangecount_c(transval, minval, maxval);
  Datum loguess, higuess, curguess;
  int64 curcount, best = 0;
  float8 oldpct = 0, curpct = 0, centile = (float8)intcentile/100.0;
  int i;

  /* binary search over the domain */  
  curguess = ValGetDatum(0, transval->elementTypeOid);
  for (i = 0, loguess = minval, higuess = maxval; 
        i < TYPLEN(transval->elementTypeOid) - 1; i++) {
    curcount = cmsketch_rangecount_c(transval, minval, curguess);
    // elog(NOTICE, "curgess: %ld, curcount: %ld, curpct: %f", 
    //       (int64)DatumGetVal(curguess, transval->elementTypeOid),
    //       curcount, curpct);
    curpct = ((float8)curcount / (float8)total);
    if (curpct > centile) {
      // overshot
      if ((curpct - centile) < ABS(oldpct - centile)) {
        best = (int64)DatumGetVal(curguess, transval->elementTypeOid);
        oldpct = curpct;
      }
      higuess = curguess;
      curguess = loguess + (curguess - loguess) / 2;
    }
    else if (curpct < centile) {
      // undershot
      if ((centile - curpct) < ABS(oldpct - centile)) {
        best = (int64)DatumGetVal(curguess, transval->elementTypeOid);
        oldpct = curpct;
      }
      loguess = curguess;
      curguess = higuess - (higuess - curguess) / 2;
    }
    else {
      // the exact median!
      best = curguess;
      break;
    }
  }
  PG_RETURN_INT64(best);
}

PG_FUNCTION_INFO_V1(cmsketch_histogram);
// UDF wrapper.  All the interesting stuff is in fmsketch_getcount_c
Datum cmsketch_histogram(PG_FUNCTION_ARGS)
{
  bytea          *transblob = (bytea *)PG_GETARG_BYTEA_P(0);
  cmtransval     *transval = (cmtransval *)VARDATA(transblob);
  int            translen = VARSIZE(PG_GETARG_BYTEA_P(0));
  int            buckets = PG_GETARG_INT32(1);
  Datum          retval;
    
  // elog(NOTICE, "transval is %d long", translen);
  if (translen == sizeof(cmtransval) + ROWS*COLUMNS*sizeof(int64) + VARHDRSZ) {
    elog(WARNING, "Cannot compute histogram for a non-integer type");
    PG_RETURN_NULL();
  }
  
  retval = cmsketch_histogram_c(transval, buckets);
  PG_RETURN_DATUM(retval);
}

Datum cmsketch_histogram_c(cmtransval *transval, int buckets)
{
  Datum minval = MINVAL(transval->elementTypeOid);
  Datum maxval = MAXVAL(transval->elementTypeOid);
  Oid   typeOid = transval->elementTypeOid;
  int64 total = cmsketch_rangecount_c(transval, minval, maxval);
  Datum curguess = MIDVAL(transval->elementTypeOid);
  Datum loguess, higuess;
  int64 curcount, min, max, step;
  int i;
  ArrayType      *retval;
  int64 binlo, binhi, binval;
  Datum histo[buckets][3];
  int dims[2], lbs[2];
       
  for (i = 0, loguess = minval, higuess = maxval;
       (i < TYPLEN(transval->elementTypeOid) - 1) && (higuess-loguess > 1); i++) {
     curcount = cmsketch_rangecount_c(transval, minval, curguess);
     // elog(NOTICE, "curgess: %ld, curcount: %ld, curpct: %f", 
     //      (int64)DatumGetVal(curguess, transval->elementTypeOid),
     //      curcount, curpct);
     if (curcount > 0) {
      // overshot
      higuess = curguess;
      curguess = loguess + (curguess - loguess) / 2;
     }
     else {
      // probably undershot
      loguess = curguess;
      curguess = higuess - (higuess - curguess) / 2;
     }
   }
   min = (cmsketch_rangecount_c(transval, minval, curguess) == 0) ? 
             (int64)DatumGetValTypeOid(curguess) : (int64)DatumGetValTypeOid(loguess);
   min++; // we know that min is below the minimum, so bump it by one
   
   for (i = loguess = 0, higuess = maxval;
        (i < TYPLEN(transval->elementTypeOid) - 1) && (higuess-loguess > 1); i++) {
    curcount = cmsketch_rangecount_c(transval, minval, curguess);
    // elog(NOTICE, "curgess: %ld, curcount: %ld, curpct: %f", 
    //      (int64)DatumGetVal(curguess, transval->elementTypeOid),
    //      curcount, curpct);
    if (curcount == total) {
     // probably overshot
     higuess = curguess;
     curguess = loguess + (curguess - loguess) / 2;
    }
    else {
     // probably undershot
     loguess = curguess;
     curguess = higuess - (higuess - curguess) / 2;
    }
  }
  max = (cmsketch_rangecount_c(transval, minval, curguess) == total) ? 
            (int64)DatumGetValTypeOid(curguess) : DatumGetValTypeOid(higuess);
  // don't decrement the max: it may be the true max.  We could find the true max 
  // but let's not bother for now.

  elog(NOTICE, "min: %Ld, max: %Ld", (long long)min, (long long)max);
  
  step = MAX(trunc((float8)(max-min+1) / (float8)buckets), 1);
  for (i = 0; i < buckets; i++) {
    binlo = min + i*step;
    if (binlo > max) break;
    histo[i][0] = ValGetDatumTypeOid(binlo);
    binhi = (i == buckets-1) ? max : (min + (i+1)*step - 1);
    histo[i][1] = ValGetDatumTypeOid(binhi);
    binval = cmsketch_rangecount_c(transval, histo[i][0], histo[i][1]);
    histo[i][2] = ValGetDatumTypeOid(binval);
    // elog(NOTICE, "bucket [%ld, %ld]: %ld", binlo, binhi, binval);
  }
  
  
  dims[0] = i; // may be less than requested buckets if too few values
  dims[1] = 3; // lo, hi, and val
  lbs[0] = 0;
  lbs[1] = 0;
  
  retval = construct_md_array((Datum *)histo, NULL, 
                              2, dims, lbs, INT8OID, sizeof(int64), true, 'd');
  PG_RETURN_ARRAYTYPE_P(retval);
}

PG_FUNCTION_INFO_V1(cmsketch_combine);
// UDF wrapper.  All the interesting stuff is in fmsketch_getcount_c
Datum cmsketch_combine(PG_FUNCTION_ARGS)
{
  bytea          *counterblob1 = (bytea *)PG_GETARG_BYTEA_P(0);
  bytea          *counterblob2 = (bytea *)PG_GETARG_BYTEA_P(1);
  int64          *counters1 = (int64 *)VARDATA(counterblob1);
  int64          *counters2 = (int64 *)VARDATA(counterblob2);
  bytea          *newblob;
  int64          *newcounters;
  int            i;
  int            sz = MAX(VARSIZE(counterblob1), VARSIZE(counterblob2));

  // elog(NOTICE, "pallocing %d in cmsketch_combine", sz);
  newblob = (bytea *)palloc(sz);
  SET_VARSIZE(newblob, sz);
  newcounters = (int64 *)VARDATA(newblob);

  if (VARSIZE(counterblob2) == VARHDRSZ)
    memcpy(newblob, counterblob1, VARSIZE(counterblob1));
  else if (VARSIZE(counterblob1) == VARHDRSZ)
    memcpy(newblob, counterblob2, VARSIZE(counterblob2));    
  else if (VARSIZE(counterblob1) == VARSIZE(counterblob2)) {
    for (i = 0; i < (VARSIZE(newblob) - VARHDRSZ)/sizeof(int64); i++)
      newcounters[i] = counters1[i] + counters2[i];
  }
  else {
    elog(ERROR, "Attempting to combine sketches of different sizes: %d and %d.", 
         VARSIZE(counterblob1), VARSIZE(counterblob2));
    PG_RETURN_NULL();
  }

  PG_RETURN_DATUM(PointerGetDatum(newblob)); 
}


PG_FUNCTION_INFO_V1(cmsketch_dump);

// UDF wrapper.  All the interesting stuff is in fmsketch_dump_c
Datum cmsketch_dump(PG_FUNCTION_ARGS)
{
  bytea          *transblob = (bytea *)PG_GETARG_BYTEA_P(0);
  int64          *counters;
  char           *newblob = (char *)palloc(10240);
  int            i, j;
  
  counters = ((cmtransval *)VARDATA(transblob))->counters;
  for (i=0, j=0; i < (VARSIZE(transblob) - VARHDRSZ)/sizeof(int64); i++) {
    if (counters[i] != 0)
      j += sprintf(&newblob[j], "[%d:%ld], ", i, counters[i]);
	if (j > 10000) break;
  }
  newblob[j] = '\0';
  elog(NOTICE, "nonzero entries [index:val]: %s", newblob);
  PG_RETURN_NULL();
}
