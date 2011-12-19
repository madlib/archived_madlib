/* ----------------------------------------------------------------------- *//**
 *
 * @file greenplum/dbconnector/Compatibility.cpp
 *
 *//* ----------------------------------------------------------------------- */

extern "C" {
    #include <postgres.h>
    #include <funcapi.h>
    #include <nodes/execnodes.h>
} // extern "C"

// Include the header from the PostgreSQL port
#include <dbconnector/Compatibility.hpp>

namespace madlib {

namespace dbconnector {

namespace postgres {

/**
 * @brief Test whether we are currently in an aggregate calling context.
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
    if (fcinfo->context && IsA(fcinfo->context, WindowState)) {
		if (aggcontext)
			*aggcontext = ((WindowState *) fcinfo->context)->transcontext;
		return AGG_CONTEXT_WINDOW;
	}

	/* this is just to prevent "uninitialized variable" warnings */
	if (aggcontext)
		*aggcontext = NULL;
	return 0;
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib
