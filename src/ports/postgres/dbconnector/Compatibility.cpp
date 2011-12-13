/* ----------------------------------------------------------------------- *//**
 *
 * @file Compatibility.cpp
 *
 * @brief Compatibility with old PostgreSQL versions
 *
 * This file is only used in the PostgreSQL port, not derived ports (like
 * Greenplum).
 *
 *//* ----------------------------------------------------------------------- */

extern "C" {
    #include <postgres.h>
    #include <funcapi.h>
    #include <nodes/execnodes.h>
} // extern "C"

#include "Compatibility.hpp"

namespace madlib {

namespace dbconnector {

namespace postgres {

#if PG_VERSION_NUM < 90000

/**
 * @brief Test whether we are currently in an aggregate calling context.
 * 
 * Knowing whether we are in an aggregate calling context is useful, because it
 * allows write access to the transition state of the aggregate function.
 * At all other time, modifying a pass-by-reference input is strictly forbidden:
 * http://developer.postgresql.org/pgdocs/postgres/xaggr.html
 *
 * This function is essentially a copy of AggCheckCallContext from
 * backend/executor/nodeAgg.c, which is part of PostgreSQL >= 9.0.
 */
int AggCheckCallContext(FunctionCallInfo fcinfo, MemoryContext *aggcontext) {
	if (fcinfo->context && IsA(fcinfo->context, AggState)) {
		if (aggcontext)
			*aggcontext = ((AggState *) fcinfo->context)->aggcontext;
		return AGG_CONTEXT_AGGREGATE;
	}

    /* More recent versions of PostgreSQL also have a window aggregate context.
     * However, these changes are not contained in the 8.4 branch (or before).
     *
     * Reference: See changed file src/include/nodes/execnodes.h from
     * commit ec4be2ee6827b6bd85e0813c7a8993cfbb0e6fa7 from
     * Fri, 12 Feb 2010 17:33:21 +0000 (17:33 +0000)
     * by Tom Lane <tgl@sss.pgh.pa.us> */
    
	/* this is just to prevent "uninitialized variable" warnings */
	if (aggcontext)
		*aggcontext = NULL;
	return 0;
}

#endif

} // namespace postgres

} // namespace dbconnector

} // namespace madlib
