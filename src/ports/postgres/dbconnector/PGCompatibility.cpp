/* ----------------------------------------------------------------------- *//**
 *
 * @file PGCompatibility.cpp
 *
 * This file is only used in the PostgreSQL port, not derived ports (like
 * Greenplum).
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/PGCompatibility.hpp>

extern "C" {
    #include <funcapi.h>
    #include <nodes/execnodes.h>
} // extern "C"

namespace madlib {

namespace dbconnector {

#if PG_VERSION_NUM < 90000

/**
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

} // namespace dbconnector

} // namespace madlib
