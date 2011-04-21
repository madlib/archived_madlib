/* ----------------------------------------------------------------------- *//**
 *
 * @file compatibility.cpp
 *
 *//* ----------------------------------------------------------------------- */

#include <madlib/ports/postgres/compatibility.hpp>

extern "C" {
    #include <funcapi.h>
    #include <utils/builtins.h>
} // extern "C"

namespace madlib {

namespace ports {

namespace postgres {

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

#if PG_VERSION_NUM >= 80400
	if (fcinfo->context && IsA(fcinfo->context, WindowAggState)) {
		if (aggcontext)
			*aggcontext = ((WindowAggState *) fcinfo->context)->aggcontext;
		return AGG_CONTEXT_WINDOW;
	}
#elif defined(GP_VERSION_NUM)
    if (fcinfo->context && IsA(fcinfo->context, WindowState)) {
		if (aggcontext)
			*aggcontext = ((WindowState *) fcinfo->context)->transcontext;
		return AGG_CONTEXT_WINDOW;
	}
#endif

	/* this is just to prevent "uninitialized variable" warnings */
	if (aggcontext)
		*aggcontext = NULL;
	return 0;
}

#endif

} // namespace postgres

} // namespace ports

} // namespace madlib
