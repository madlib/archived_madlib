/* ----------------------------------------------------------------------- *//**
 *
 * @file Compatibility.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_COMPATIBILITY_HPP
#define MADLIB_POSTGRES_COMPATIBILITY_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

namespace {
// No need to make these function accessible outside of the postgres namespace.

#ifndef FLOAT8ARRAYOID
    #define FLOAT8ARRAYOID 1022
#endif

#ifndef PG_GET_COLLATION
// See madlib_InitFunctionCallInfoData()
#define PG_GET_COLLATION()	InvalidOid
#endif

#ifndef SearchSysCache1
// See madlib_SearchSysCache1()
#define SearchSysCache1(cacheId, key1) \
	SearchSysCache(cacheId, key1, 0, 0, 0)
#endif

/*
 * In commit 2d4db3675fa7a2f4831b755bc98242421901042f,
 * by Tom Lane <tgl@sss.pgh.pa.us> Wed, 6 Jun 2007 23:00:50 +0000,
 * is_array_type was changed to type_is_array
 */
#if defined(is_array_type) && !defined(type_is_array)
    #define type_is_array(x) is_array_type(x)
#endif

#if PG_VERSION_NUM < 90000

/**
 * The following has existed in PostgresSQL since commit ID
 * d5768dce10576c2fb1254c03fb29475d4fac6bb4, by
 * Tom Lane <tgl@sss.pgh.pa.us>	Mon, 8 Feb 2010 20:39:52 +0000.
 */

/* AggCheckCallContext can return one of the following codes, or 0: */
#define AGG_CONTEXT_AGGREGATE	1		/* regular aggregate */
#define AGG_CONTEXT_WINDOW		2		/* window function */

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
inline
int
AggCheckCallContext(FunctionCallInfo fcinfo, MemoryContext *aggcontext) {
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

#endif // PG_VERSION_NUM < 90000

} // namnespace

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_COMPATIBILITY_HPP)
