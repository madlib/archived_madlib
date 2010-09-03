#include "postgres.h"
#include "utils/array.h"
#include "utils/elog.h"
#include "nodes/execnodes.h"
#include "fmgr.h"
#include "sketch_support.h"

// already used in fm.c.  Uncomment to compile without fm.c.
// #ifdef PG_MODULE_MAGIC
// PG_MODULE_MAGIC;
// #endif

PG_FUNCTION_INFO_V1(sketch_array_set_bit_in_place);
Datum sketch_array_set_bit_in_place(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(sketch_rightmost_one);
Datum sketch_rightmost_one(PG_FUNCTION_ARGS);
PG_FUNCTION_INFO_V1(sketch_leftmost_zero);
Datum sketch_leftmost_zero(PG_FUNCTION_ARGS);

Datum sketch_rightmost_one(PG_FUNCTION_ARGS)
{
  bytea          *bitmap = (bytea *)PG_GETARG_BYTEA_P(0);
  size_t            sketchsz = PG_GETARG_INT32(1); // size in bits
  size_t            sketchnum = PG_GETARG_INT32(2); // from the left!
  char           *bits = VARDATA(bitmap);
  size_t          len = VARSIZE_ANY_EXHDR(bitmap);
  
  return rightmost_one((unsigned char *)bits, len, sketchsz, sketchnum);
}

Datum sketch_leftmost_zero(PG_FUNCTION_ARGS)
{
  bytea          *bitmap = (bytea *)PG_GETARG_BYTEA_P(0);
  size_t            sketchsz = PG_GETARG_INT32(1); // size in bits
  size_t            sketchnum = PG_GETARG_INT32(2); // from the left!
  char           *bits = VARDATA(bitmap);
  size_t          len = VARSIZE_ANY_EXHDR(bitmap);
  
  return leftmost_zero((unsigned char *)bits, len, sketchsz, sketchnum);
}

Datum sketch_array_set_bit_in_place(PG_FUNCTION_ARGS)
{

  bytea           *bitmap = (bytea *)PG_GETARG_BYTEA_P(0);
  int4            numsketches = PG_GETARG_INT32(1);
  int4            sketchsz = PG_GETARG_INT32(2); // size in bits
  int4            sketchnum = PG_GETARG_INT32(3); // from the left!
  int4            bitnum = PG_GETARG_INT32(4); // from the right!
  
  return array_set_bit_in_place(bitmap, numsketches, sketchsz, sketchnum, bitnum);
}
