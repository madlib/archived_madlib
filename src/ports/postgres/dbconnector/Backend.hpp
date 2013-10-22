/* ----------------------------------------------------------------------- *//**
 *
 * @file Backend.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_BACKEND_HPP
#define MADLIB_POSTGRES_BACKEND_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

namespace {
// No need to make these function accessible outside of the postgres namespace.

MADLIB_WRAP_PG_FUNC(
    bool, type_is_array, (Oid typid), (typid))

MADLIB_WRAP_PG_FUNC(
    AclResult, pg_proc_aclcheck, (Oid proc_oid, Oid roleid, AclMode mode),
    (proc_oid, roleid, mode))

MADLIB_WRAP_PG_FUNC(
    void*, MemoryContextAlloc, (MemoryContext context, Size size),
    (context, size))

MADLIB_WRAP_PG_FUNC(
    void*, MemoryContextAllocZero, (MemoryContext context, Size size),
    (context, size))

MADLIB_WRAP_PG_FUNC(
    char*, format_procedure, (Oid procedure_oid), (procedure_oid))

MADLIB_WRAP_PG_FUNC(
    Oid, get_fn_expr_argtype, (FmgrInfo* flinfo, int argnum), (flinfo, argnum))

MADLIB_WRAP_PG_FUNC(
    TypeFuncClass, get_call_result_type, (FunctionCallInfo fcinfo,
        Oid* resultTypeId, TupleDesc* resultTupleDesc),
    (fcinfo, resultTypeId, resultTupleDesc))

MADLIB_WRAP_PG_FUNC(
    HeapTupleHeader, DatumGetHeapTupleHeader, (Datum d), (d))

MADLIB_WRAP_PG_FUNC(
    bytea*, DatumGetByteaPCopy, (Datum d), (d))

MADLIB_WRAP_PG_FUNC(
    ArrayType*, DatumGetArrayTypePCopy, (Datum d), (d))

MADLIB_WRAP_PG_FUNC(
    Datum, GetAttributeByNum, (HeapTupleHeader tuple, AttrNumber attrno,
        bool* isNull),
    (tuple, attrno, isNull))

MADLIB_WRAP_VOID_PG_FUNC(
    fmgr_info_cxt, (Oid functionId, FmgrInfo* finfo, MemoryContext mcxt),
    (functionId, finfo, mcxt))

MADLIB_WRAP_PG_FUNC(
    HeapTuple, heap_form_tuple, (TupleDesc tupleDescriptor, Datum* values,
        bool* isnull),
    (tupleDescriptor, values, isnull))

MADLIB_WRAP_PG_FUNC(
    HTAB*, hash_create, (const char* tabname, long nelem, HASHCTL* info,
        int flags),
    (tabname, nelem, info, flags))

MADLIB_WRAP_PG_FUNC(
    void*, hash_search, (HTAB* hashp, const void* keyPtr, HASHACTION action,
        bool* foundPtr),
    (hashp, keyPtr, action, foundPtr))

// Calls to SearchSysCache and related functions have been wrapped using macros
// with commit e26c539e by Robert Haas <rhaas@postgresql.org>
// on Sun, 14 Feb 2010 18:42:19 UTC. First release: PG9.0.
MADLIB_WRAP_PG_FUNC(
    HeapTuple, SearchSysCache1, (int cacheId, Datum key1), (cacheId, key1))

MADLIB_WRAP_PG_FUNC(
    TupleDesc, lookup_rowtype_tupdesc_copy, (Oid type_id, int32 typmod),
    (type_id, typmod))

MADLIB_WRAP_VOID_PG_FUNC(
    ReleaseSysCache, (HeapTuple tuple), (tuple))

MADLIB_WRAP_PG_FUNC(
    Datum, SysCacheGetAttr, (int cacheId, HeapTuple tup,
        AttrNumber attributeNumber, bool* isNull),
    (cacheId, tup, attributeNumber, isNull))

MADLIB_WRAP_PG_FUNC(
    struct varlena*, pg_detoast_datum, (struct varlena* datum), (datum))

MADLIB_WRAP_VOID_PG_FUNC(
    get_typlenbyvalalign,
    (Oid typid, int16 *typlen, bool *typbyval, char *typalign),
    (typid, typlen, typbyval, typalign)
    )

inline
void
madlib_InitFunctionCallInfoData(FunctionCallInfoData& fcinfo, FmgrInfo* flinfo,
    short nargs, Oid fncollation, fmNodePtr context, fmNodePtr resultinfo) {

#if PG_VERSION_NUM >= 90100
    // Collation support has been added to PostgreSQL with commit
    // d64713df by Tom Lane <tgl@sss.pgh.pa.us>
    // on Tue Apr 12 2011 23:19:24 UTC. First release: PG9.1.
    InitFunctionCallInfoData(fcinfo, flinfo, nargs, fncollation, context,
        resultinfo);
#else
    (void) fncollation;
    InitFunctionCallInfoData(fcinfo, flinfo, nargs, context, resultinfo);
#endif
}

template <typename T>
inline
T*
madlib_detoast_verlena_datum_if_necessary(Datum inDatum) {
    varlena* ptr = reinterpret_cast<varlena*>(DatumGetPointer(inDatum));

    if (!VARATT_IS_EXTENDED(ptr))
        return reinterpret_cast<T*>(ptr);
    else
        return reinterpret_cast<T*>(madlib_pg_detoast_datum(ptr));
}

/**
 * @brief Convert a Datum into a bytea
 *
 * For performance reasons, we look into the varlena struct in order to check
 * if we can avoid a PG_TRY block.
 */
inline
bytea*
madlib_DatumGetByteaP(Datum inDatum) {
    return madlib_detoast_verlena_datum_if_necessary<bytea>(inDatum);
}

/**
 * @brief Convert a Datum into an ArrayType
 *
 * For performance reasons, we look into the varlena struct in order to check
 * if we can avoid a PG_TRY block.
 */
inline
ArrayType*
madlib_DatumGetArrayTypeP(Datum inDatum) {
    ArrayType* x = madlib_detoast_verlena_datum_if_necessary<ArrayType>(inDatum);
    if (ARR_HASNULL(x)) {
        // an empty array has dimensionality 0
        size_t array_size = ARR_NDIM(x) ? 1 : 0;
        for (int i = 0; i < ARR_NDIM(x); i ++) {
            array_size *= ARR_DIMS(x)[i];
        }

        throw ArrayWithNullException(array_size);
    }

    return x;
}

} // namespace

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_BACKEND_ABSTRACTION_HPP)
