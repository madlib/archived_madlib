/* ----------------------------------------------------------------------- *//**
 *
 * @file greenplum/dbconnector/Compatibility.cpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_GREENPLUM_COMPATIBILITY_HPP
#define MADLIB_GREENPLUM_COMPATIBILITY_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

namespace {
// No need to make these function accessible outside of the postgres namespace.

#ifndef PG_GET_COLLATION
// Greenplum does not currently have support for collations
#define PG_GET_COLLATION()	InvalidOid
#endif

#ifndef SearchSysCache1
// See madlib_SearchSysCache1()
#define SearchSysCache1(cacheId, key1) \
	SearchSysCache(cacheId, key1, 0, 0, 0)
#endif

#if (GP_VERSION_NUM < 40399)
/*
 * In commit 2d4db3675fa7a2f4831b755bc98242421901042f,
 * by Tom Lane <tgl@sss.pgh.pa.us> Wed, 6 Jun 2007 23:00:50 +0000,
 * is_array_type was changed to type_is_array
 */
#if defined(is_array_type) && !defined(type_is_array)
    #define type_is_array(x) is_array_type(x)
#endif

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
 * This function is essentially a copy of AggCheckCallContext from
 * backend/executor/nodeAgg.c, which is part of PostgreSQL >= 9.0.
 *
 * @warning The AggState struct is known to have changed between GPDB 4.2.1 and
 *     GPDB 4.2.2. Essentially, this imnplies that it is not safe to ever rely
 *     on what \c aggcontext will be set to!
 */
inline
int
AggCheckCallContext(FunctionCallInfo fcinfo, MemoryContext *aggcontext) {
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
#endif // GP_VERSION_NUM < 40399
} // namespace

inline ArrayType* madlib_construct_array
(
    Datum*  elems,
    int     nelems,
    Oid     elmtype,
    int     elmlen,
    bool    elmbyval,
    char    elmalign
){
    return 
        construct_array(
            elems, nelems, elmtype, elmlen, elmbyval, elmalign);
}

inline ArrayType* madlib_construct_md_array
(
    Datum*  elems,
    bool*   nulls,
    int     ndims,
    int*    dims,
    int*    lbs,
    Oid     elmtype,
    int     elmlen,
    bool    elmbyval,
    char    elmalign
){
    return
        construct_md_array(
            elems, nulls, ndims, dims, lbs, elmtype, elmlen, elmbyval,
            elmalign);
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_GREENPLUM_COMPATIBILITY_HPP)
