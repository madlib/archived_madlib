/*-------------------------------------------------------------------------
 *
 * student.h
 *
 * Evaluate the Student-T distribution function.
 *
 *------------------------------------------------------------------------- 
 */

#ifndef STUDENT_H
#define STUDENT_H

#include "postgres.h"
#include "fmgr.h"

extern float8 studentT_cdf(uint32 /* nu */, float8 /* t */);

extern Datum student_t_cdf(PG_FUNCTION_ARGS);


#endif