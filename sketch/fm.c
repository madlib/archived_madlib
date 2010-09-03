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

#include <ctype.h>

#ifdef PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

#define NMAP 256
#define FMSKETCH_SZ (VARHDRSZ + NMAP*(HASHLEN_BITS)/CHAR_BIT)

#define SORTASORT_SPACE 8*MINVALS
#define SORTA_SLOP 100

// empirically, estimates seem to fall below 1% error around 12k distinct vals
#define MINVALS 1024*12

// because FM sketches work poorly on small numbers of values,
// our transval can be in one of two modes.
// for "SMALL" numbers of values (<=MINVALS), the storage array 
// is a "sortasort" data structure containing an array of input values.
// for "BIG" datasets (>MINVAL), it is an array of FM sketch bitmaps.
typedef struct {  
  enum {SMALL,BIG} status;
  char storage[0];
} fmtransval;

// For small datasets, we do not use an FM sketch; we just want to keep 
// an array of values seen so far.
// A "sortasort" is a smallish array of strings, intended for append-only 
// modification, and network transmission as a single byte-string.  It is 
// structured as an array of offsets (directory) that point to the actual 
// null-terminated strings stored in the "vals" array at the end of the structure.  
// The directory is mostly sorted in ascending order of the vals it points 
// to, but the last < SORTA_SLOP entries are left unsorted.  Binary Search is used
// on all but those last entries, which must be scanned. At every k*SORTA_SLOP'th 
// insert, the full directory is sorted.
typedef struct {
  short     num_vals;
  size_t    storage_sz;
  unsigned  storage_cur;
  unsigned  dir[MINVALS];
  char      vals[0];
} sortasort;

Datum fmsketch_iter_c(bytea *, char *);
Datum fmsketch_getcount_c(bytea *);
Datum fmsketch_iter(PG_FUNCTION_ARGS);
Datum fmsketch_getcount(PG_FUNCTION_ARGS);
Datum fmsketch_merge(PG_FUNCTION_ARGS);
Datum big_or(bytea *bitmap1, bytea *bitmap2);
bytea *sortasort_insert(bytea *, char *);
sortasort *sortasort_init(sortasort *, size_t);
int sortasort_find(sortasort *, char *);
int sorta_cmp(const void *i, const void *j, void *thunk);

PG_FUNCTION_INFO_V1(fmsketch_iter);

// This is the UDF interface.  It just converts args into C strings and 
// calls the interesting logic in fmsketch_iter_c
Datum fmsketch_iter(PG_FUNCTION_ARGS)
{
  bytea          *transblob = (bytea *)PG_GETARG_BYTEA_P(0);
  fmtransval     *transval;
  Oid            element_type = get_fn_expr_argtype(fcinfo->flinfo, 1);
  Oid            funcOid;
  bool           typIsVarlena;
  char           *string;
  
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


  /* get the provided element, being careful in case it's NULL */
  if (!PG_ARGISNULL(1)) {
    // figure out the outfunc for this type
    getTypeOutputInfo(element_type, &funcOid, &typIsVarlena);
    
    // convert input to a cstring
    string = OidOutputFunctionCall(funcOid, PG_GETARG_DATUM(1));

    // if this is the first call, initialize transval to hold a sortasort
    if (VARSIZE(transblob) <= VARHDRSZ+8) {
      transblob = (bytea *)palloc(VARHDRSZ + sizeof(fmtransval) + sizeof(sortasort) + SORTASORT_SPACE);
      SET_VARSIZE(transblob, VARHDRSZ + sizeof(fmtransval) + sizeof(sortasort) + SORTASORT_SPACE);
      transval = (fmtransval *)VARDATA(transblob);
      transval->status = SMALL;
      sortasort_init((sortasort *)transval->storage, SORTASORT_SPACE);
    }
    else transval = (fmtransval *)VARDATA(transblob);
    
    // if we've seen < MINVALS distinct values, place string into the sortasort
    if (transval->status == SMALL && ((sortasort *)(transval->storage))->num_vals < MINVALS) {
      PG_RETURN_DATUM(PointerGetDatum(sortasort_insert(transblob, string)));
    }
    
    // if we've seen exactly MINVALS distinct values, create FM bitmaps 
    // and load the contents of the sortasort into the FM sketch
    else if (transval->status == SMALL 
             && ((sortasort *)(transval->storage))->num_vals == MINVALS) {
      int i;
      sortasort *s = (sortasort *)(transval->storage);
      bytea *newblob = (bytea *)palloc(VARHDRSZ + sizeof(fmtransval) + FMSKETCH_SZ);
      SET_VARSIZE(newblob, VARHDRSZ + sizeof(fmtransval) + FMSKETCH_SZ);
      transval = (fmtransval *)VARDATA(newblob);
      transval->status = BIG; 
      MemSet(VARDATA((bytea *)transval->storage), 0, FMSKETCH_SZ - VARHDRSZ);
      SET_VARSIZE((bytea *)transval->storage, FMSKETCH_SZ);
      for (i = 0; i < MINVALS; i++)
        fmsketch_iter_c(newblob, &(s->vals[s->dir[i]]));
//      not sure how to do this ... the memory allocator doesn't like pfreeing the transval
//      pfree(transblob);
      PG_RETURN_DATUM(fmsketch_iter_c(newblob, string));
    }
        
    // else if we've seen >MINVALS distinct values, just apply FM to this string
    else PG_RETURN_DATUM(fmsketch_iter_c(transblob, string));
  }
  else PG_RETURN_NULL(); 
}

// Main loop of Flajolet and Martin's sketching algorithm.
// For each call, we get an md5 hash of the value passed in.
// First we use the hash to choose one of the NMAP bitmaps at random to update.
// Then we find the position "rmost" of the rightmost 1 bit in the hashed value.
// We then turn on the "rmost"-th bit FROM THE LEFT in the chosen bitmap.
Datum fmsketch_iter_c(bytea *transblob, char *input) 
{
  fmtransval     *transval = (fmtransval *) VARDATA(transblob);
  bytea          *bitmaps = (bytea *)transval->storage;
  uint64          index = 0, tmp;
  unsigned char   c[HASHLEN_BITS/CHAR_BIT+1];
  int             rmost;
  // unsigned int hashes[HASHLEN_BITS/sizeof(unsigned int)];
  int             i;
  bytea           *digest;
  Datum           result;

  result = DirectFunctionCall1(md5_bytea, PointerGetDatum(cstring_to_text(input)) /* CStringGetTextDatum(input) */);
  digest = (bytea *)DatumGetPointer(result);
  hex_to_bytes((char *)VARDATA(digest), c, (HASHLEN_BITS/CHAR_BIT)*2);
  
  
  /* choose a bitmap by taking the 64 high-order bits worth of hash value mod NMAP */
  /* index = get_bytes(c,0,7) % NMAP; */
  for (i=7, index = 0; i >= 0; i--) {
    tmp = (uint64)(c[i]);
    index += (tmp << i);
  }
  index = index % NMAP;

  /*
   * During the insertion we insert each element
   * in one bitmap only (a la Flajolet pseudocode, page 16).
   * Find index of the rightmost non-0 bit.  Turn on that bit (from left!) in the sketch.
   */
  rmost = rightmost_one(c, 1, HASHLEN_BITS, 0);
  result = array_set_bit_in_place(bitmaps, NMAP, HASHLEN_BITS, index, (HASHLEN_BITS - 1) - rmost);
  return PointerGetDatum(transblob);
}

PG_FUNCTION_INFO_V1(fmsketch_getcount);

// UDF wrapper.  All the interesting stuff is in fmsketch_getcount_c
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

Datum fmsketch_merge(PG_FUNCTION_ARGS)
{
  bytea *transblob1 = (bytea *)PG_GETARG_BYTEA_P(0);
  bytea *transblob2 = (bytea *)PG_GETARG_BYTEA_P(1);
  fmtransval *transval1 = (fmtransval *)VARDATA(transblob1);
  fmtransval *transval2 = (fmtransval *)VARDATA(transblob2);
  fmtransval *tval_small;
  sortasort *s1, *s2;
  sortasort *sortashort;
  bytea *tblob_big, *tblob_small;
  int i;
  
  if (transval1->status == BIG && transval2->status == BIG) {
    // elog(NOTICE, "merging two big");
    PG_RETURN_DATUM(big_or(transblob1, transblob2));
  }
  else if (transval1->status == SMALL && transval2->status == SMALL) {
    // would be more efficient to merge sorted prefixes, but not worth it
    s1 = (sortasort *)(transval1->storage);
    s2 = (sortasort *)(transval2->storage);
    // elog(NOTICE, "merging shorts: %d with %d", s1->num_vals, s2->num_vals);
    tblob_big = (s1->num_vals > s2->num_vals) ? transblob1 : transblob2;
    tblob_small = (s1->num_vals <= s2->num_vals) ? transblob2 : transblob1;
    sortashort = (s1->num_vals <= s2->num_vals) ? s1 : s2;
    for (i = 0; i < sortashort->num_vals; i++)
      tblob_big = sortasort_insert(tblob_big, &(sortashort->vals[sortashort->dir[i]]));
    PG_RETURN_DATUM(PointerGetDatum(tblob_big));
  }
  else {
    // one of each
    tval_small = (transval1->status == SMALL) ? transval1 : transval2;
    tblob_big = (transval1->status == BIG) ? transblob1 : transblob2;
    sortashort = (sortasort *)tval_small->storage;
    // elog(NOTICE, "merging one of each: %d into a sketch", sortashort->num_vals);
    for (i = 0; i < sortashort->num_vals; i++)
      fmsketch_iter_c(tblob_big, &(sortashort->vals[sortashort->dir[i]]));
//  not sure how to do this ... the memory allocator doesn't like pfreeing the transval
//  pfree(transblob);
    PG_RETURN_DATUM(PointerGetDatum(tblob_big));
  }
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


/*
******** Routines for the sortasort structur
*/
/* given a pre-allocated sortasort, set up its metadata */
sortasort *sortasort_init(sortasort *s, size_t sz)
{
  // start out with 8 bytes per entry 
  s->storage_sz = sz;
  s->num_vals = 0;
  s->storage_cur = 0;
  return(s);
}

/* comparison function for qsort_arg */
int sorta_cmp(const void *i, const void *j, void *thunk)
{
  sortasort *s = (sortasort *)thunk;
  int first = *(int *)i;
  int second = *(int *)j;
  return strcmp(&(s->vals[first]), &(s->vals[second]));
}

bytea *sortasort_insert(bytea *transblob, char *v)
{
  sortasort *s_in = (sortasort *)((fmtransval *)VARDATA(transblob))->storage;
  sortasort *s = NULL;
  bytea *newblob;
  
  int found = sortasort_find(s_in, v);
  if (found >= 0 && found < s_in->num_vals) {
    /* found!  just return the unmodified transblob */
    return(transblob);
  }
  /* sanity check */
  if (found < -1 || found >= s_in->num_vals)
    elog(ERROR, "inconsistent return %d from sortasort_find", found);
  
  // we need to insert v.  make sure there's enough space!
  if (s_in->storage_cur + strlen(v) + 1 >= s_in->storage_sz) {
    // Insufficient space
    // allocate a fmtransval with double-big storage area plus room for v
    // we can't use repalloc because it fails trying to free the old transblob
    size_t newsize = VARHDRSZ + sizeof(fmtransval) + sizeof(sortasort) + strlen(v) + s_in->storage_sz*2;
    newblob = (bytea *)palloc(newsize);
    memcpy(newblob, transblob, VARSIZE(transblob));
    SET_VARSIZE(newblob, newsize);
    s = (sortasort *)((fmtransval *)VARDATA(newblob))->storage;
    s->storage_sz = s->storage_sz*2 + strlen(v);
    // Can't figure out how to make pfree happy with transblob
    // pfree(transblob);
  }
  else {
    // if fits.  from now on we'll use the variables s and newblob, so just copy the pointers
    s = s_in;
    newblob = transblob;
  }
  
  // copy v to the current location in vals, put a pointer into dir, 
  // update num_vals and storage_cur
  memcpy(&(s->vals[s->storage_cur]), v, strlen(v) + 1); // +1 to pick up the '\0'
  s->dir[s->num_vals++] = s->storage_cur;
  s->storage_cur += strlen(v) + 1;
  if (s->storage_cur > s->storage_sz)
    elog(ERROR, "went off the end of sortasort storage");
  
  // re-sort every SORTA_SLOP vals
  if (s->num_vals % SORTA_SLOP == 0)
    qsort_arg(s->dir, s->num_vals, sizeof(unsigned), sorta_cmp, (void *)s);
  
  return(newblob);
}

// finding items in a sortasort involves binary search in the sorted prefix,
// and linear search in the <SORTA_SLOP-sized suffix.
// Returns position where it's found, or -1 if not found.
// NOTE: return value 0 does not mean false!!  It means it *found* the item
//       at position 0
int sortasort_find(sortasort *s, char *v)
{
  int guess, diff;
  int hi = (s->num_vals/SORTA_SLOP)*SORTA_SLOP;
  int min = 0, max = hi;
  int i;

  // binary search on the front of the sortasort
  if (max > s->num_vals)
    elog(ERROR, "max = %d, num_vals = %d", max, s->num_vals);
  guess = hi / 2;
  while (min < (max - 1)) {
    if (!(diff = strcmp(&(s->vals[s->dir[guess]]), v)))
      return guess;
    else if (diff < 0) {
      // undershot
      min = guess;
      guess += (max - guess) / 2;
    }
    else {
      // overshot
      max = guess;
      guess -= (guess - min) / 2;
    }
  }
  
  // if we got here, continue with a naive linear search on the tail
  for (i = hi; i < s->num_vals; i++)
    if (!strcmp(&(s->vals[s->dir[i]]), v))
      return i;
  return -1;      
}
