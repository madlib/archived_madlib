// Flajolet-Martin JCSS 1985 distinct count estimation 
// implemented as a user-defined aggregate.  
// See http://algo.inria.fr/flajolet/Publications/FlMa85.pdf 
// for explanation and pseudocode

/* THIS CODE MAY NEED TO BE REVISITED TO ENSURE ALIGNMENT! */

#include "postgres.h"
#include "utils/array.h"
#include "utils/elog.h"
#include "utils/builtins.h"
#include "utils/lsyscache.h"
#include "libpq/md5.h"
#include "nodes/execnodes.h"
#include "fmgr.h"
#include "sketch_support.h"
#include "sortasort.h"
#include <ctype.h>

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#define NMAP 256
#define FMSKETCH_SZ (VARHDRSZ + NMAP*(HASHLEN_BITS)/CHAR_BIT)
#define MINVALS 1024*12

// initial size for a sortasort: we'll guess at 8 bytes per string.
// sortasort will grow dynamically if we guessed too low
#define SORTASORT_INITIAL_STORAGE  sizeof(sortasort) + MINVALS*sizeof(uint32) + 8*MINVALS

// because FM sketches work poorly on small numbers of values,
// our transval can be in one of two modes.
// for "SMALL" numbers of values (<=MINVALS), the storage array 
// is a "sortasort" data structure containing an array of input values.
// for "BIG" datasets (>MINVAL), it is an array of FM sketch bitmaps.
typedef struct {  
  enum {SMALL,BIG} status;
  char storage[0];
} fmtransval;

Datum fmsketch_trans_c(bytea *, char *);
Datum fmsketch_getcount_c(bytea *);
Datum fmsketch_trans(PG_FUNCTION_ARGS);
Datum fmsketch_getcount(PG_FUNCTION_ARGS);
Datum fmsketch_merge(PG_FUNCTION_ARGS);
Datum big_or(bytea *bitmap1, bytea *bitmap2);
bytea *fmsketch_sortasort_insert(bytea *, char *);
bytea *fm_new(void);

PG_FUNCTION_INFO_V1(fmsketch_trans);

// UDA transition function for the fmsketch aggregate.
Datum fmsketch_trans(PG_FUNCTION_ARGS)
{
  bytea          *transblob = (bytea *)PG_GETARG_BYTEA_P(0);
  fmtransval     *transval;
  Oid            element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
  Oid            funcOid;
  bool           typIsVarlena;
  char           *string;
  
  if (!OidIsValid(element_type))
      elog(ERROR, "could not determine data type of input");

  // This is Postgres boilerplate for UDFs that modify the data in their own context.
  // Such UDFs can only be correctly called in an agg context since regular scalar
  // UDFs are essentially stateless across invocations.
  if (!(fcinfo->context &&
                  (IsA(fcinfo->context, AggState) 
    #ifdef NOTGP
                   || IsA(fcinfo->context, WindowAggState)
    #endif
    	      )))
          elog(ERROR, "destructive pass by reference outside agg");


  /* get the provided element, being careful in case it's NULL */
  if (!PG_ARGISNULL(1)) {
    // XXX POTENTIAL BUG HERE
    // We are hashing based on the string produced by the type's outfunc.
    //      This may not produce the right answer, if the outfunc doesn't produce
    //      a distinct string for every distinct value.  
    // XXX CHECK HOW HASH JOIN DOES THIS.
    
    // figure out the outfunc for this type
    getTypeOutputInfo(element_type, &funcOid, &typIsVarlena);
    
    // convert input to a cstring
    string = OidOutputFunctionCall(funcOid, PG_GETARG_DATUM(1));

    // if this is the first call, initialize transval to hold a sortasort
    if (VARSIZE(transblob) <= VARHDRSZ+8) {
      size_t blobsz = VARHDRSZ + sizeof(fmtransval) + SORTASORT_INITIAL_STORAGE;
      transblob = (bytea *)palloc(blobsz);
      SET_VARSIZE(transblob, blobsz);
      transval = (fmtransval *)VARDATA(transblob);
      transval->status = SMALL;
      sortasort_init((sortasort *)transval->storage, MINVALS, SORTASORT_INITIAL_STORAGE);
    }
    else {
      // extract the existing transval from the transblob
      transval = (fmtransval *)VARDATA(transblob);
    }
    
    // if we've seen < MINVALS distinct values, place string into the sortasort
    if (transval->status == SMALL 
        && ((sortasort *)(transval->storage))->num_vals < MINVALS) {
      PG_RETURN_DATUM(PointerGetDatum(fmsketch_sortasort_insert(transblob, string)));
    }
    
    // if we've seen exactly MINVALS distinct values, create FM bitmaps 
    // and load the contents of the sortasort into the FM sketch
    else if (transval->status == SMALL 
             && ((sortasort *)(transval->storage))->num_vals == MINVALS) {
      int i;
      sortasort *s = (sortasort *)(transval->storage);
      bytea *newblob = fm_new();
      transval = (fmtransval *)VARDATA(newblob);
      
      // apply the FM sketching algorithm to each value previously stored in the sortasort
      for (i = 0; i < MINVALS; i++)
        fmsketch_trans_c(newblob, SORTASORT_GETVAL(s,i));
        
      // XXXX would like to pfree the old transblob, but the memory allocator doesn't like it

      // drop through to insert the current string in "BIG" mode
      transblob = newblob;
    }
        
    // if we're here we've seen >=MINVALS distinct values and are in BIG mode. 
    // Apply FM algorithm to this string
    if (transval->status != BIG)
      elog(ERROR, "FM sketch with more than min vals marked as SMALL");
    PG_RETURN_DATUM(fmsketch_trans_c(transblob, string));
  }
  else PG_RETURN_NULL(); 
}

bytea *fm_new()
{
  bytea *newblob = (bytea *)palloc(VARHDRSZ + sizeof(fmtransval) + FMSKETCH_SZ);
  fmtransval *transval;

  SET_VARSIZE(newblob, VARHDRSZ + sizeof(fmtransval) + FMSKETCH_SZ);
  transval = (fmtransval *)VARDATA(newblob);
  transval->status = BIG; 

  // zero out a new array of FM Sketch bitmaps
  MemSet(VARDATA((bytea *)transval->storage), 0, FMSKETCH_SZ - VARHDRSZ);
  SET_VARSIZE((bytea *)transval->storage, FMSKETCH_SZ);
  return(newblob);
}

// Main logic of Flajolet and Martin's sketching algorithm.
// For each call, we get an md5 hash of the value passed in.
// First we use the hash as a random number to choose one of 
// the NMAP bitmaps at random to update.
// Then we find the position "rmost" of the rightmost 1 bit in the hashed value.
// We then turn on the "rmost"-th bit FROM THE LEFT in the chosen bitmap.
Datum fmsketch_trans_c(bytea *transblob, char *input) 
{
  fmtransval     *transval = (fmtransval *) VARDATA(transblob);
  bytea          *bitmaps = (bytea *)transval->storage;
  uint64          index;
  unsigned char   c[HASHLEN_BITS/CHAR_BIT+1];
  int             rmost;
  // unsigned int hashes[HASHLEN_BITS/sizeof(unsigned int)];
  bytea           *digest;
  Datum           result;

  // The POSTGRES code for md5 returns a bytea with a textual representation of the 
  // md5 result.  We then convert it back into binary.
  // XXX The internal POSTGRES source code is actually converting from binary to the bytea
  //     so this is rather wasteful, but the internal code is marked static and unavailable here.
  result = DirectFunctionCall1(md5_bytea, PointerGetDatum(cstring_to_text(input)) /* CStringGetTextDatum(input) */);
  digest = (bytea *)DatumGetPointer(result);
  
  // XXXX Why does the 3rd argument work here?
  hex_to_bytes((char *)VARDATA(digest), c, (HASHLEN_BITS/CHAR_BIT)*2);
    
  /*    
   * During the insertion we insert each element
   * in one bitmap only (a la Flajolet pseudocode, page 16).  
   * Choose the bitmap by taking the 64 high-order bits worth of hash value mod NMAP 
   */
  index = (*(uint64 *)c) % NMAP;  

  /*
   * Find index of the rightmost non-0 bit.  Turn on that bit (from left!) in the sketch.
   */
  rmost = rightmost_one(c, 1, HASHLEN_BITS, 0);
  
  // last argument must be the index of the bit position from the right.
  // i.e. position 0 is the rightmost.
  // so to set the bit at rmost from the left, we subtract from the total number of bits.
  result = array_set_bit_in_place(bitmaps, NMAP, HASHLEN_BITS, index, (HASHLEN_BITS - 1) - rmost);
  return PointerGetDatum(transblob);
}

PG_FUNCTION_INFO_V1(fmsketch_getcount);

// UDA final function to get count(distinct) out of an FM sketch
Datum fmsketch_getcount(PG_FUNCTION_ARGS)
{
  fmtransval     *transval = (fmtransval *)VARDATA((PG_GETARG_BYTEA_P(0)));
  
  // if status is not BIG then get count from htab
  if (transval->status == SMALL) 
    return ((sortasort *)(transval->storage))->num_vals;
  // else get count via fm
  else 
    return fmsketch_getcount_c((bytea *)transval->storage);
}

// Finish up the Flajolet-Martin approximation.
// We sum up the number of leading 1 bits across all bitmaps in the sketch.
// Then we use the FM magic formula to estimate the distinct count.
Datum fmsketch_getcount_c(bytea *bitmaps)
{
//  int R = 0; // Flajolet/Martin's R is handled by leftmost_zero
  unsigned int S = 0;
  static double phi = 0.77351; /* the magic constant */
  /* char out[NMAP*HASHLEN_BITS]; */
  int i;
  unsigned int lz;
  
  for (i = 0; i < NMAP; i++)
  {
    lz = leftmost_zero((unsigned char *)VARDATA(bitmaps), NMAP, HASHLEN_BITS, i);
    S = S + lz;
  }
  
  PG_RETURN_INT64(ceil( ((double)NMAP/phi) * pow(2.0, (double)S/(double)NMAP) ));
}


PG_FUNCTION_INFO_V1(fmsketch_merge);

// Greenplum "prefunc": a function to merge 2 transvals computed at different machines.
// For simple FM, this is trivial: just OR together the two arrays of bitmaps.
// But we have to deal with cases where one or both transval is SMALL: i.e. it
// holds a sortasort, not an FM sketch.
//
// THIS IS NOT WELL TESTED -- need to exercise all branches.
Datum fmsketch_merge(PG_FUNCTION_ARGS)
{
  bytea *transblob1 = (bytea *)PG_GETARG_BYTEA_P(0);
  bytea *transblob2 = (bytea *)PG_GETARG_BYTEA_P(1);
  fmtransval *transval1, *transval2;
  sortasort *s1, *s2;
  sortasort *sortashort, *sortabig;
  bytea *tblob_big, *tblob_small;
  int i;
  
  // deal with the case where one or both items is the initial value of ''
  if (VARSIZE(transblob1) == VARHDRSZ) {
    // elog(NOTICE, "transblob1 is empty");
    PG_RETURN_DATUM(PointerGetDatum(transblob2));
  }
  if (VARSIZE(transblob2) == VARHDRSZ) {
    // elog(NOTICE, "transblob1 is empty");
    PG_RETURN_DATUM(PointerGetDatum(transblob1));
  }

  transval1 = (fmtransval *)VARDATA(transblob1);
  transval2 = (fmtransval *)VARDATA(transblob2);

  if (transval1->status == BIG && transval2->status == BIG) {
    // easy case: merge two FM sketches via bitwise OR.
    PG_RETURN_DATUM(big_or(transblob1, transblob2));
  }  
  else if (transval1->status == SMALL && transval2->status == SMALL) {
    s1 = (sortasort *)(transval1->storage);
    s2 = (sortasort *)(transval2->storage);
    tblob_big = (s1->num_vals > s2->num_vals) ? transblob1 : transblob2;
    tblob_small = (s1->num_vals <= s2->num_vals) ? transblob2 : transblob1;
    sortashort = (s1->num_vals <= s2->num_vals) ? s1 : s2;
    sortabig = (s1->num_vals <= s2->num_vals) ? s2 : s1;
    if (sortabig->num_vals + sortashort->num_vals <= sortabig->capacity) {
      // we have room in sortabig
      // one could imagine a more efficient (merge-based) sortasort merge,
      // but for now we just copy the values from the smaller sortasort into
      // the bigger one.
      for (i = 0; i < sortashort->num_vals; i++)
        tblob_big = fmsketch_sortasort_insert(tblob_big, SORTASORT_GETVAL(sortashort,i));
      PG_RETURN_DATUM(PointerGetDatum(tblob_big));
    }
    // else drop through.
  }

  // if we got here, then one or both transval is SMALL.
  // need to form an FM sketch and populate with the SMALL transval(s)
  if (transval1->status == SMALL && transval2->status == SMALL)
    tblob_big = fm_new();
  else 
    tblob_big = (transval1->status == BIG) ? transblob1 : transblob2;
    
  if (transval1->status == SMALL) {
    s1 = (sortasort *)(transval1->storage);
    for(i = 0; i < s1->num_vals; i++) {
      fmsketch_trans_c(tblob_big, SORTASORT_GETVAL(s1,i));
    }
  }
  if (transval2->status == SMALL) {
    s2 = (sortasort *)(transval2->storage);
    for(i = 0; i < s2->num_vals; i++) {
      fmsketch_trans_c(tblob_big, SORTASORT_GETVAL(s2,i));
    }
  }
  PG_RETURN_DATUM(PointerGetDatum(tblob_big));
}

// OR of two big bitmaps, for gathering sketches computed in parallel.
Datum big_or(bytea *bitmap1, bytea *bitmap2)
{
  bytea *out;
  unsigned int i;
  
  if (VARSIZE(bitmap1) != VARSIZE(bitmap2))
    elog(ERROR, "attempting to OR two different-sized bitmaps: %d, %d", VARSIZE(bitmap1), VARSIZE(bitmap2));
  
  out = palloc(VARSIZE(bitmap1));
  SET_VARSIZE(out, VARSIZE(bitmap1));

  for (i=0; i < VARSIZE(bitmap1) - VARHDRSZ; i++)
    ((char *)(VARDATA(out)))[i] = ((char *)(VARDATA(bitmap1)))[i] | ((char *)(VARDATA(bitmap2)))[i];
  
  PG_RETURN_BYTEA_P(out);
}

bytea *fmsketch_sortasort_insert(bytea *transblob, char *v)
{
  sortasort *s_in = (sortasort *)((fmtransval *)VARDATA(transblob))->storage;
  bytea *newblob;
  bool  success = FALSE;
  size_t newsize;
  
  if (s_in->num_vals >= s_in->capacity)
    elog(ERROR, "attempt to insert into full sortasort");
    
  success = sortasort_try_insert(s_in, v);
  if (success < 0)
    elog(ERROR, "insufficient directory capacity in sortasort");
    
  if (success == TRUE) return (transblob);
  
  while (!success) {
    // else insufficient space
    // allocate a fmtransval with double-big storage area plus room for v
    // should work 2nd time around the loop.
    // we can't use repalloc because it fails trying to free the old transblob
    newsize = VARHDRSZ + sizeof(fmtransval) + sizeof(sortasort)
              + s_in->capacity*sizeof(s_in->dir[0]) + s_in->storage_sz*2 + strlen(v);
    newblob = (bytea *)palloc(newsize);
    memcpy(newblob, transblob, VARSIZE(transblob));
    SET_VARSIZE(newblob, newsize);
    s_in = (sortasort *)((fmtransval *)VARDATA(newblob))->storage;
    s_in->storage_sz = s_in->storage_sz*2 + strlen(v);
    // Can't figure out how to make pfree happy with transblob
    // pfree(transblob);
    transblob = newblob;
    success = sortasort_try_insert(s_in, v);
  }
  return(transblob);
}
