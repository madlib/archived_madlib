/* ----------------------------------------------------------------------- *//**
 *
 * @file Compatibility.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_COMPATIBILITY_HPP
#define MADLIB_POSTGRES_COMPATIBILITY_HPP

extern "C" {
    #include <access/tupmacs.h>
    #include <utils/memutils.h>

    #if PG_VERSION_NUM >= 90300
        #include <access/htup_details.h>
    #endif
}


namespace madlib {

namespace dbconnector {

namespace postgres {

namespace {
// No need to make these function accessible outside of the postgres namespace.

#ifndef FLOAT8ARRAYOID
    #define FLOAT8ARRAYOID 1022
#endif

#ifndef INT8ARRAYOID
    #define INT8ARRAYOID 1016
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

} // namespace


/**
 * @brief construct an array of zero values.
 * @note the supported types are: int2, int4, int8, float4 and float8
 *
 */
static ArrayType* construct_md_array_zero
(
    int     ndims,
    int*    dims,
    int*    lbs,
    Oid     elmtype,
    int     elmlen,
    bool    elmbyval,
    char    elmalign
){
    ArrayType  *result;
    size_t      nbytes;
    int32       dataoffset;
    int         i;
    int         nelems;
    Datum       theDatum = Datum(0);
    (void) elmbyval;
    if (ndims < 0)              /* we do allow zero-dimension arrays */
        ereport(ERROR,
                (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                 errmsg("invalid number of dimensions: %d", ndims)));
    if (ndims > MAXDIM)
        ereport(ERROR,
                (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
                 errmsg("number of array dimensions (%d) exceeds the maximum allowed (%d)",
                        ndims, MAXDIM)));

    /* fast track for empty array */
    if (ndims == 0)
        return construct_empty_array(elmtype);

    nelems = ArrayGetNItems(ndims, dims);

    /* compute required space */
    nbytes = 0;

    switch (elmtype)
    {
        case INT2OID:
            theDatum = Int16GetDatum(1);
            break;
        case INT4OID:
            theDatum = Int32GetDatum(1);
            break;
        case INT8OID:
            theDatum = Int64GetDatum(1);
            break;
        case FLOAT4OID:
            theDatum = Float4GetDatum(1.0);
            break;
        case FLOAT8OID:
            theDatum = Float8GetDatum(1.0);
            break;
        default:
            ereport(ERROR,
                    (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
                     errmsg("the support types are INT2, INT4, INT8, FLOAT4 and FLOAT8")));
            break;
    }
    for (i = 0; i < nelems; i++)
    {
        /* make sure data is not toasted */
        if (elmlen == -1)
            theDatum = PointerGetDatum(PG_DETOAST_DATUM(theDatum));
        nbytes = att_addlength_datum(nbytes, elmlen, theDatum);
        nbytes = att_align_nominal(nbytes, elmalign);
        /* check for overflow of total request */
        if (!AllocSizeIsValid(nbytes))
            ereport(ERROR,
                    (errcode(ERRCODE_PROGRAM_LIMIT_EXCEEDED),
                     errmsg("array size exceeds the maximum allowed (%d)",
                            (int) MaxAllocSize)));
    }

    dataoffset = 0;         /* marker for no null bitmap */
    nbytes += ARR_OVERHEAD_NONULLS(ndims);
    result = (ArrayType *) palloc0(nbytes);
    SET_VARSIZE(result, nbytes);
    result->ndim = ndims;
    result->dataoffset = dataoffset;
    result->elemtype = elmtype;
    memcpy(ARR_DIMS(result), dims, ndims * sizeof(int));
    memcpy(ARR_LBOUND(result), lbs, ndims * sizeof(int));

    return result;
}

/**
 * @brief construct an array of zero values.
 * @note the supported types are: int2, int4, int8, float4 and float8
 */
static ArrayType* construct_array_zero
(
    int     nelems,
    Oid     elmtype,
    int     elmlen,
    bool    elmbyval,
    char    elmalign
)
{
    int         dims[1];
    int         lbs[1];

    dims[0] = nelems;
    lbs[0] = 1;

    return
        construct_md_array_zero(
            1, dims, lbs, elmtype, elmlen, elmbyval, elmalign);
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
        elems ?  
        construct_md_array(
            elems, nulls, ndims, dims, lbs, elmtype, elmlen, elmbyval,
            elmalign) :
        construct_md_array_zero(
            ndims, dims, lbs, elmtype, elmlen, elmbyval, elmalign); 
}

inline ArrayType* madlib_construct_array
(
    Datum*  elems,
    int     nelems,
    Oid     elmtype,
    int     elmlen,
    bool    elmbyval,
    char    elmalign
){
    return elems ?
           construct_array(elems, nelems, elmtype, elmlen, elmbyval, elmalign) :
           construct_array_zero(nelems, elmtype, elmlen, elmbyval, elmalign);
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_COMPATIBILITY_HPP)
