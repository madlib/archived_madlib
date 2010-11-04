/*-------------------------------------------------------------------------
 *
 * pinv.h
 *
 * Compute the pseudo-inverse of a matrix.
 *
 *------------------------------------------------------------------------- 
 */

#ifndef MADLIB_PINV_H
#define MADLIB_PINV_H

#include "postgres.h"
#include "fmgr.h"

#ifdef __APPLE__
	#include <vecLib/clapack.h>

	typedef __CLPK_integer integer;

	// The following lines are copied from f2c.h, which is not present on Mac OS X.
	#define min(a,b) ((a) <= (b) ? (a) : (b))
	#define max(a,b) ((a) >= (b) ? (a) : (b))
#else
	#ifndef WIN32
		/* Includes from the clapack library */
		#include <f2c.h>
		#include <clapack.h>
	#endif
#endif


Datum pseudoinverse(PG_FUNCTION_ARGS);
void pinv(integer rows, integer columns, float8 *A, float8 *Aplus);

#endif
