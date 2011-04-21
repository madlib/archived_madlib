#ifndef MADLIB_PGCOMPATIBILITY_HPP

#include <madlib/ports/postgres/postgres.hpp>

extern "C" {
    #include <fmgr.h>
    #include <utils/lsyscache.h>    // because of type_is_array
} // extern "C"

namespace madlib {

namespace ports {

namespace postgres {

#if PG_VERSION_NUM < 90000

/**
 * The following has existed in PostgresSQL since commit ID
 * d5768dce10576c2fb1254c03fb29475d4fac6bb4, by
 * Tom Lane <tgl@sss.pgh.pa.us>	Mon, 8 Feb 2010 20:39:52 +0000.
 */

/* AggCheckCallContext can return one of the following codes, or 0: */
#define AGG_CONTEXT_AGGREGATE	1		/* regular aggregate */
#define AGG_CONTEXT_WINDOW		2		/* window function */

int AggCheckCallContext(FunctionCallInfo fcinfo, MemoryContext *aggcontext);

/*
 * In commit 2d4db3675fa7a2f4831b755bc98242421901042f,
 * by Tom Lane <tgl@sss.pgh.pa.us> Wed, 6 Jun 2007 23:00:50 +0000,
 * is_array_type was changed to type_is_array
 */
#if defined(is_array_type) && !defined(type_is_array)
    #define type_is_array(x) is_array_type(x)
#endif

#endif // PG_VERSION_NUM < 90000

} // namespace postgres

} // namespace ports

} // namespace madlib

#endif
