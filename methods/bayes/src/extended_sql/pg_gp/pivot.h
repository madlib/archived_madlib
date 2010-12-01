/*-------------------------------------------------------------------------
 *
 * pivot.h
 *
 * Naive Bayes Classification.
 *
 *------------------------------------------------------------------------- 
 */

#ifndef MADLIB_PIVOT_H
#define MADLIB_PIVOT_H

#include "postgres.h"
#include "fmgr.h"

extern Datum int4_pivot_accum(PG_FUNCTION_ARGS);
extern Datum int8_pivot_accum(PG_FUNCTION_ARGS);
extern Datum float8_pivot_accum(PG_FUNCTION_ARGS);
extern Datum text_unpivot(PG_FUNCTION_ARGS);

#endif
