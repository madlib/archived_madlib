/* ----------------------------------------------------------------------- *//**
 *
 * @file SystemInformation_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_SYSTEMINFORMATION_IMPL_HPP
#define MADLIB_POSTGRES_SYSTEMINFORMATION_IMPL_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

namespace {

/**
 * @brief Initialize an OID-to-data hash table
 */
inline
void
initializeOidHashTable(HTAB*& ioHashTable, MemoryContext inCacheContext,
    size_t inEntrySize, const char* inTabName, uint32_t inMaxNElems) {

    if (ioHashTable == NULL) {
        HASHCTL ctl;
        ctl.keysize = sizeof(Oid);
        ctl.entrysize = inEntrySize;
        ctl.hash = oid_hash;
        ctl.hcxt = inCacheContext;
        ioHashTable = madlib_hash_create(
            /* tabname -- a name for the table (for debugging purposes) */
            inTabName,
            /* nelem -- maximum number of elements expected */
            inMaxNElems,
            /* info: additional table parameters, as indicated by flags */
            &ctl,
            /* flags -- bitmask indicating which parameters to take from *info */
            HASH_ELEM | HASH_FUNCTION | HASH_CONTEXT
        );
    }
}

/**
 * @brief Store cached system-catalog information in backend function handle
 *
 * @param inFmgrInfo Backend handle to the function
 * @param inSysInfo Our system-catalog information that should be stored
 *
 * A set-returning function uses \c fn_extra to store cross-call information.
 * See, e.g., init_MultiFuncCall() in funcapi.c. Fortunately, it stores a point
 * to a <tt>struct FuncCallContext</tt> in \c fn_extra, which in turn allows to
 * store user-defined data.
 */
inline
void
setSystemInformationInFmgrInfo(FmgrInfo* inFmgrInfo,
    SystemInformation* inSysInfo) {

    (inFmgrInfo->fn_retset
        ? static_cast<FuncCallContext*>(inFmgrInfo->fn_extra)->user_fctx
        : inFmgrInfo->fn_extra)
        = inSysInfo;
}

/**
 * @brief Get cached system-catalog information from backend function handle
 *
 * @see setSystemInformationInFmgrInfo()
 */
inline
SystemInformation*
getSystemInformationFromFmgrInfo(FmgrInfo* inFmgrInfo) {
    return static_cast<SystemInformation*>(inFmgrInfo->fn_retset
        ? static_cast<FuncCallContext*>(inFmgrInfo->fn_extra)->user_fctx
        : inFmgrInfo->fn_extra);
}

/**
 * @brief Get memory context from backend function handle
 *
 * The memory context returned may be used for storing user-defined data. In
 * SystemInformation::get(), we will use it for allocating a
 * <tt>struct SystemInformation</tt>.
 *
 * @see setSystemInformationInFmgrInfo()
 */
inline
MemoryContext
getMemoryContextFromFmgrInfo(FmgrInfo* inFmgrInfo) {
    return inFmgrInfo->fn_retset
        ? static_cast<FuncCallContext*>(inFmgrInfo->fn_extra)
            ->multi_call_memory_ctx
        : inFmgrInfo->fn_mcxt;
}

} // namespace

/**
 * @brief Get (and cache) information from the PostgreSQL catalog
 *
 * @param inFmgrInfo System-catalog information about the function. If no
 *     SystemInformation is currently available in struct FmgrInfo, this
 *     function will store it (and thus change the FmgrInfo struct).
 */
inline
SystemInformation*
SystemInformation::get(FunctionCallInfo fcinfo) {
    madlib_assert(fcinfo->flinfo,
        std::invalid_argument("Incomplete FunctionCallInfoData."));

    SystemInformation* sysInfo
        = getSystemInformationFromFmgrInfo(fcinfo->flinfo);

    if (!sysInfo) {
        MemoryContext memCtxt = getMemoryContextFromFmgrInfo(fcinfo->flinfo);

        sysInfo = static_cast<SystemInformation*>(
            madlib_MemoryContextAllocZero(
                memCtxt, sizeof(SystemInformation)));
        sysInfo->entryFuncOID = fcinfo->flinfo->fn_oid;
        sysInfo->cacheContext = memCtxt;
        sysInfo->collationOID = PG_GET_COLLATION();
        setSystemInformationInFmgrInfo(fcinfo->flinfo, sysInfo);
    }
    return sysInfo;
}

/**
 * @brief Get (and cache) information about a PostgreSQL type
 *
 * @param inPGInfo An PGInformation structure containing all cached information
 * @param inTypeID The OID of the type of interest
 */
inline
TypeInformation*
SystemInformation::typeInformation(Oid inTypeID) {
    TypeInformation* cachedTypeInfo = NULL;
    bool found = true;
    MemoryContext oldContext;
    HeapTuple tup;
    Form_pg_type pgType;

    // We arrange to look up info about types only once per series of
    // calls, assuming the type info doesn't change underneath us.
    initializeOidHashTable(types, cacheContext,
        sizeof(TypeInformation),
        "C++ AL / TypeInformation hash table",
        12);

    // BACKEND: Since we pass HASH_FIND, this function call will never perform
    // an allocation. There is nothing in the code path that would raise an
    // exception (including oid_hash()), so we are *not* wrapping in a PG_TRY()
    // block for performance reasons.
    cachedTypeInfo = static_cast<TypeInformation*>(
        hash_search(types, &inTypeID, HASH_FIND, &found));

    if (!found) {
        cachedTypeInfo = static_cast<TypeInformation*>(
            madlib_hash_search(types, &inTypeID, HASH_ENTER, &found));
        // cachedTypeInfo.oid is already set
        tup = madlib_SearchSysCache1(TYPEOID, ObjectIdGetDatum(inTypeID));
        // BACKEND: HeapTupleIsValid is just a macro
        if (!HeapTupleIsValid(tup)) {
            throw std::runtime_error("Error while looking up a type in the "
                "system catalog.");
        } else {
            // BACKEND: GETSTRUCT is just a macro
            pgType = reinterpret_cast<Form_pg_type>(GETSTRUCT(tup));
            std::strncpy(cachedTypeInfo->name, pgType->typname.data,
                NAMEDATALEN);
            cachedTypeInfo->len = pgType->typlen;
            cachedTypeInfo->byval = pgType->typbyval;
            cachedTypeInfo->type = pgType->typtype;

            if (cachedTypeInfo->type == TYPTYPE_COMPOSITE) {
                // BACKEND: MemoryContextSwitchTo just changes a global
                // variable
                oldContext = MemoryContextSwitchTo(cacheContext);
                // Since type ID != RECORDOID, typmod will not be used and
                // we can set it to -1
                // (RECORDOID is a pseudo type and used for transient record
                // types. They are identified by an index in array
                // RecordCacheArray defined in typcache.c.)
                cachedTypeInfo->tupdesc = madlib_lookup_rowtype_tupdesc_copy(
                    /* type_id */ inTypeID,
                    /* typmod */ -1);
                MemoryContextSwitchTo(oldContext);
            } else {
                cachedTypeInfo->tupdesc = NULL;
            }
            madlib_ReleaseSysCache(tup);
        }
    }

    return cachedTypeInfo;
}

/**
 * @brief Get (and cache) information about a PostgreSQL function
 *
 * @param inFuncID The OID of the function of interest
 * @return
 */
inline
FunctionInformation*
SystemInformation::functionInformation(Oid inFuncID) {
    FunctionInformation* cachedFuncInfo = NULL;
    bool found = true;
    HeapTuple tup;
    Form_pg_proc pgFunc;

    // We arrange to look up info about functions only once per series of
    // calls, assuming the function info doesn't change underneath us.
    initializeOidHashTable(functions, cacheContext,
        sizeof(FunctionInformation),
        "C++ AL / FunctionInformation hash table",
        8);

    // BACKEND: Since we pass HASH_FIND, this function call will never perform
    // an allocation. There is nothing in the code path that would raise an
    // exception (including oid_hash()), so we are *not* wrapping in a PG_TRY()
    // block for performance reasons.
    cachedFuncInfo = static_cast<FunctionInformation*>(
        hash_search(functions, &inFuncID, HASH_FIND, &found));

    if (!found) {
        cachedFuncInfo = static_cast<FunctionInformation*>(
            madlib_hash_search(functions, &inFuncID, HASH_ENTER, &found));
        // cachedFuncInfo.oid is already set
        cachedFuncInfo->mSysInfo = this;
        tup = madlib_SearchSysCache1(PROCOID, ObjectIdGetDatum(inFuncID));
        if (!HeapTupleIsValid(tup)) {
            throw std::runtime_error("Error while looking up a function in the "
                "system catalog.");
        } else {
            // BACKEND: GETSTRUCT is just a macro
            pgFunc = reinterpret_cast<Form_pg_proc>(GETSTRUCT(tup));
            cachedFuncInfo->cxx_func = NULL;
            cachedFuncInfo->flinfo.fn_oid = InvalidOid;
            // The number of arguments (excluding OUT params)
            cachedFuncInfo->nargs
                = static_cast<uint16_t>(pgFunc->proargtypes.dim1);
            cachedFuncInfo->polymorphic = false;
            cachedFuncInfo->isstrict = pgFunc->proisstrict;
            cachedFuncInfo->secdef = pgFunc->prosecdef;

            Oid* allargs;
            // We could use get_func_arg_info() but unfortunately that also
            // copied names and modes
            bool onlyINArguments = false;
            Datum allargtypes = madlib_SysCacheGetAttr(PROCOID, tup,
                Anum_pg_proc_proallargtypes, &onlyINArguments);

            if (onlyINArguments) {
                allargs = pgFunc->proargtypes.values;
            } else {
                // See get_func_arg_info(). We expect the arrays to be 1-D
                // arrays of the right types; verify that.
                // Ensure that array is not toasted. We do not worry about
                // a possible memory leak here. This code will be run only
                // once per entry function (into the C++ AL) and query, and
                // memory will already be garbage collected once the currenty
                // entry point is left.
                ArrayType* arr = madlib_DatumGetArrayTypeP(allargtypes);
                int numargs = ARR_DIMS(arr)[0];
                madlib_assert(ARR_NDIM(arr) == 1 && ARR_DIMS(arr)[0] >= 0 &&
                    !ARR_HASNULL(arr) && ARR_ELEMTYPE(arr) == OIDOID &&
                    numargs >= pgFunc->pronargs,
                    std::runtime_error("In SystemInformation::"
                        "functionInformation(): proallargtypes is not a vaid "
                        "one-dimensional Oid array"));
                allargs = reinterpret_cast<Oid*>(ARR_DATA_PTR(arr));
            }

            for (int i = 0; i < pgFunc->pronargs; ++i) {
                // Note that pgFunc->pronargs is the number of all arguments
                // (including OUT params)
                if (typeInformation(allargs[i])->getType()
                    == TYPTYPE_PSEUDO) {

                    cachedFuncInfo->polymorphic = true;
                    break;
                }
            }

            if (cachedFuncInfo->nargs == 0) {
                cachedFuncInfo->argtypes = NULL;
            } else {
                cachedFuncInfo->argtypes = static_cast<Oid*>(
                    madlib_MemoryContextAlloc(cacheContext,
                        cachedFuncInfo->nargs * sizeof(Oid)));
                std::memcpy(cachedFuncInfo->argtypes,
                    pgFunc->proargtypes.values,
                    cachedFuncInfo->nargs * sizeof(Oid));
            }

            cachedFuncInfo->rettype = pgFunc->prorettype;

            // If the return type is RECORDOID, we cannot yet determine the
            // tuple description, even if the function is not polymorphic.
            // For that, the expression parse tree is required.
            // If the return type is composite but not RECORDOID, the tuple
            // description will be stored with the type information, so no
            // need to have it here.
            cachedFuncInfo->tupdesc = NULL;
            madlib_ReleaseSysCache(tup);
        }
    }

    return cachedFuncInfo;
}

/**
 * @brief Retrieve tuple description for a composite type
 *
 * @param inTypeMod Transient record types have tye ID RECORDOID and are
 *     identified by an index in array RecordCacheArray (defined in typcache.c).
 *     This index is stored in the tdtypmod field of struct tupleDesc.
 *     See the description of \c tupleDesc in tupdesc.h.
 */
inline
TupleDesc
TypeInformation::getTupleDesc(int32_t inTypeMod) {
    if (tupdesc != NULL)
        // We have the tuple description in our cache, so return it
        return tupdesc;
    else if (oid == RECORDOID && inTypeMod >= 0) {
        // It is an anonymous type, but PostgreSQL has it in its own cache
        // In this case lookup_rowtype_tupdesc_noerror(RECORDOID, ...)
        // does currently not perform any actions that could rise an exception.
        // We might not want to rely on that, however (at the expense of
        // performance).
        TupleDesc pgCachedTupDesc = lookup_rowtype_tupdesc_noerror(oid,
            inTypeMod, /* noerror */ true);

        // The tupleDesc is in the cache (RecordCacheArray defined in
        // typcache.c) even before lookup_rowtype... is called. There is no
        // need to release the tupleDesc, though we do in order to avoid any
        // side effect.
        ReleaseTupleDesc(pgCachedTupDesc);
        return pgCachedTupDesc;
    }

    return NULL;
}

/**
 * @brief Determine whether a specified type is composite
 *
 * This is our cached replacement for type_is_rowtype() in lsyscache.c
 */
inline
bool
TypeInformation::isCompositeType() {
    return oid == RECORDOID || type == TYPTYPE_COMPOSITE;
}

/**
 * @brief Retrieve the name of the specified type
 */
inline
const char*
TypeInformation::getName() {
    return name;
}

inline
bool
TypeInformation::isByValue() {
    return byval;
}

inline
int16_t
TypeInformation::getLen() {
    return len;
}

inline
char
TypeInformation::getType() {
    return type;
}



/**
 * @brief Retrieve the type of a function argument
 *
 * @param inArgID The ID of the argument (first argument is 0)
 * @param inFmgrInfo System-catalog information about the function. For
 *     polymorphic functions, this should be non-NULL and contain the
 *     expression parse tree.
 * @return The OID of the type of the argument. The type ID of polymorphic
 *     arguments is resolved if possible, i.e., if the expression parse tree
 *     is available in <tt>inFmgrInfo->fn_expr</tt>.
 */
inline
Oid
FunctionInformation::getArgumentType(uint16_t inArgID, FmgrInfo* inFmgrInfo) {
    if (!inFmgrInfo)
        inFmgrInfo = getFuncMgrInfo();

    madlib_assert(inFmgrInfo && oid == inFmgrInfo->fn_oid, std::runtime_error(
        "Invalid arguments passed to FunctionInformation::getArgumentType()."));
    Oid typeID = argtypes[inArgID];
    TypeInformation* cachedTypeInfo = mSysInfo->typeInformation(typeID);

    if (cachedTypeInfo->getType() == TYPTYPE_PSEUDO
        && inFmgrInfo->fn_expr != NULL) {
        // Type is a pseudotype, so the actual type information is given
        // by the expression parse tree.
        // This would fail if no expression parse tree is present, i.e., if
        // inFmgrInfo->fn_expr == NULL.

        typeID = madlib_get_fn_expr_argtype(inFmgrInfo, inArgID);
    }

    return typeID;
}

/**
 * @brief Retrieve the function return type
 *
 * @param fcinfo Information about function call, including the function OID
 * @return The actual return type (i.e., with resolved pseudo-types)
 */
inline
Oid
FunctionInformation::getReturnType(FunctionCallInfo fcinfo) {
    madlib_assert(fcinfo->flinfo && oid == fcinfo->flinfo->fn_oid,
        std::runtime_error("Invalid arguments passed to "
            "FunctionInformation::getReturnType()."));

    Oid returnType = rettype;
    if (rettype != RECORDOID &&
        mSysInfo->typeInformation(rettype)->type == TYPTYPE_PSEUDO) {
        // The function is polymorphic, and the result type thus depends on the
        // expression parse tree. Note that the condition in the if-clause is
        // sufficient condition for cachedFuncInfo->polymorphic, but not a
        // necessary condition. (A function could have input arguments with
        // pseudo types, but a fixed return type.)

        madlib_assert(polymorphic,
            std::logic_error("Logical error: Function returns non-record "
                "pseudo type but is not polymorphic."));

        // This is not a composite type, so no need to pass anything for
        // resultTupleDesc
        madlib_get_call_result_type(fcinfo, &returnType,
            /* resultTupleDesc */ NULL);
    }

    return returnType;
}

/**
 * @brief Retrieve the tuple description of a function's return type
 *
 * @param fcinfo Information about function call, including the function OID
 * @return The tuple description if the return type is composite and NULL
 *     otherwise
 */
inline
TupleDesc
FunctionInformation::getReturnTupleDesc(FunctionCallInfo fcinfo) {
    madlib_assert(fcinfo->flinfo && oid == fcinfo->flinfo->fn_oid,
        std::runtime_error("Invalid arguments passed to "
            "FunctionInformation::getReturnTupleDesc()."));

    TupleDesc returnTupDesc = tupdesc;
    if (returnTupDesc == NULL) {
        if (rettype == RECORDOID) {
            MADLIB_PG_TRY {
                // The return type is known, so no need to pass anything for
                // resultTypeId
                // get_call_result_type() creates the tuple description using
                // lookup_rowtype_tupdesc_copy(), which is not reference-counted
                // So there is no need to release the TupleDesc
                get_call_result_type(fcinfo, /* resultTypeId */ NULL,
                    &returnTupDesc);

                if (!polymorphic) {
                    // Since the function is not polymorphic, we can store the
                    // tuple description for other calls
                    MemoryContext oldContext
                        = MemoryContextSwitchTo(mSysInfo->cacheContext);
                    tupdesc = CreateTupleDescCopyConstr(returnTupDesc);
                    MemoryContextSwitchTo(oldContext);
                }
            } MADLIB_PG_DEFAULT_CATCH_AND_END_TRY;
        } else {
            TypeInformation* cachedTypeInfo
                = mSysInfo->typeInformation(rettype);

            if (cachedTypeInfo->type == TYPTYPE_COMPOSITE)
                returnTupDesc = cachedTypeInfo->tupdesc;
        }
    }

    return returnTupDesc;
}

/**
 * @brief Retrieve (cross-call) system-catalog information about the function
 *
 * @param inFuncID The OID of the function of interest
 * @return A filled FmgrInfo struct which uses the current SystemInformation
 *     object as system-catalog cache
 *
 * If no FmgrInfo data is stored for the function of interest, this
 * function creates a new struct FmgrInfo and stores it in the current
 * FunctionInformation object. It also links the new FmgrInfo struct to the
 * currently used SystemInformation object.
 *
 * @note Cached FmgrInfo data is *not* used for entry functions (i.e., the
 *     immediate call by the backend). For the entry function, a complete
 *     struct FunctionCallInfoData is passed by the backend itself, which is
 *     used instead (as it also contains the parse tree).
 */
inline
FmgrInfo*
FunctionInformation::getFuncMgrInfo() {
    // Initially flinfo.fn_oid is set to InvalidOid
    if (flinfo.fn_oid != oid) {
        // Check permissions
        if (madlib_pg_proc_aclcheck(oid, GetUserId(), ACL_EXECUTE)
            != ACLCHECK_OK) {

            throw std::invalid_argument(std::string("No privilege to run "
                "function '") + getFullName() + "'.");
        }

        // cacheContext will be set as fn_mcxt.
        madlib_fmgr_info_cxt(oid, &flinfo, mSysInfo->cacheContext);

        if (!secdef) {
            // If the function is SECURITY DEFINER then fmgr_info_cxt() has
            // set up flinfo so that what we will actually
            // call fmgr_security_definer() in fmgr.c, which then calls the
            // "real" function. Because of this additional layer, and since
            // fmgr_security_definer() uses the fn_extra field of
            // struct FmgrInfo in an opaque way (it points to a struct that is
            // local to fmgr.c), we only initialize the cache if the function
            // is *not* SECURITY DEFINER.
            setSystemInformationInFmgrInfo(&flinfo, mSysInfo);
        }
    }

    return &flinfo;
}

/**
 * @brief Retrieve the full function name (including arguments)
 *
 * We currently do not cache this information because we expect this function
 * to be primarily called by error handlers.
 */
inline
const char*
FunctionInformation::getFullName() {
    return madlib_format_procedure(oid);
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_SYSTEMINFORMATION_IMPL_HPP)
