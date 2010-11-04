/*-------------------------------------------------------------------------
 *
 * matrix.h
 *
 * Basic matrix functions (addition, multiplication).
 *
 *------------------------------------------------------------------------- 
 */

#ifndef MADLIB_MATRIX_H
#define MADLIB_MATRIX_H

#include "postgres.h"
#include "fmgr.h"

Datum matrix_transpose(PG_FUNCTION_ARGS);
Datum matrix_multiply(PG_FUNCTION_ARGS);
Datum matrix_add(PG_FUNCTION_ARGS);
Datum int8_matrix_smultiply(PG_FUNCTION_ARGS);
Datum float8_matrix_smultiply(PG_FUNCTION_ARGS);

#endif
