/*-------------------------------------------------------------------------
 *
 * regress.h
 *
 * Multilinear regression.
 *
 *------------------------------------------------------------------------- 
 */

#ifndef MADLIB_REGRESS_H
#define MADLIB_REGRESS_H

#include "postgres.h"
#include "fmgr.h"

Datum float8_mregr_accum(PG_FUNCTION_ARGS);
Datum float8_mregr_combine(PG_FUNCTION_ARGS);

Datum float8_mregr_coef(PG_FUNCTION_ARGS);
Datum float8_mregr_r2(PG_FUNCTION_ARGS);
Datum float8_mregr_tstats(PG_FUNCTION_ARGS);
Datum float8_mregr_pvalues(PG_FUNCTION_ARGS);

#endif
