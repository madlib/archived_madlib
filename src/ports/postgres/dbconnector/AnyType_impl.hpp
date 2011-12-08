/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyType.hpp
 *
 * @brief Convert between PostgreSQL types and C++ types
 *
 * AnyType objects are used by user-defined code to both retrieve and return
 * values from the backend.
 *
 *//* ----------------------------------------------------------------------- */

inline
AbstractionLayer::AnyType::AnyType(FunctionCallInfo inFnCallInfo)
  : mContent(FunctionComposite),
    mDatum(0),
    fcinfo(inFnCallInfo),
    mTupleHeader(NULL),
    mTypeID(InvalidOid),
    mIsMutable(false)
    { }

inline    
AbstractionLayer::AnyType::AnyType(HeapTupleHeader inTuple, Datum inDatum,
    Oid inTypeID)
  : mContent(NativeComposite),
    mDatum(inDatum),
    fcinfo(NULL),
    mTupleHeader(inTuple),
    mTypeID(inTypeID),
    mIsMutable(false)
    { }

inline
AbstractionLayer::AnyType::AnyType(Datum inDatum, Oid inTypeID,
    bool inIsMutable)
  : mContent(Scalar),
    mDatum(inDatum),
    fcinfo(NULL),
    mTupleHeader(NULL),
    mTypeID(inTypeID),
    mIsMutable(inIsMutable)
    { }

template <typename T>
inline
AbstractionLayer::AnyType::AnyType(const T &inValue)
  : mContent(Scalar),
    mDatum(TypeTraits<T>::toDatum(inValue)),
    fcinfo(NULL),
    mTupleHeader(NULL),
    mTypeID(TypeTraits<T>::oid),
    mIsMutable(false)
    { }

inline
AbstractionLayer::AnyType::AnyType()
  : mContent(Null),
    mDatum(0),
    fcinfo(NULL),
    mTupleHeader(NULL),
    mTypeID(InvalidOid),
    mIsMutable(false)
    { }

/**
 * @brief Verify consistency. Throw exception if not.
 */
inline
void
AbstractionLayer::AnyType::consistencyCheck() const {
    const char *kMsg("Inconsistency detected while converting between "
        "PostgreSQL and C++ types.");

    madlib_assert(mContent != Null || (mDatum == 0 && fcinfo == NULL &&
        mTupleHeader == NULL && mChildren.empty()),
        std::logic_error(kMsg));
    madlib_assert(mContent != FunctionComposite || fcinfo != NULL,
        std::logic_error(kMsg));
    madlib_assert(mContent != NativeComposite || mTupleHeader != NULL,
        std::logic_error(kMsg));
    madlib_assert(mContent != ReturnComposite || (!mChildren.empty() &&
        mTypeID == InvalidOid),
        std::logic_error(kMsg));
}

template <typename T>
T AbstractionLayer::AnyType::getAs() const {
    consistencyCheck();
    
    if (isNull())
        throw std::invalid_argument("Invalid type conversion requested. Got "
            "Null from backend.");
    
    if (isComposite())
        throw std::invalid_argument("Invalid type conversion requested. "
            "Expected simple or array type but got composite type from "
            "backend.");

    if (mTypeID != TypeTraits<T>::oid)
        throw std::invalid_argument(
            "Invalid type conversion requested. PostgreSQL type does not match "
                "C++ type.");
    
    if (TypeTraits<T>::isMutable && !mIsMutable)
        throw std::invalid_argument(
            "Expected argument to be mutable, but it is not.");

    return TypeTraits<T>::toCXXType(mDatum);
}

inline
bool
AbstractionLayer::AnyType::isNull() const {
    return mContent == Null;
}

inline
bool
AbstractionLayer::AnyType::isComposite() const {
    return mContent == FunctionComposite || mContent == NativeComposite ||
        mContent == ReturnComposite;
}

/**
 * @brief Internal function for determining the type of a function argument
 *
 * @internal
 *     Haveing this as separate function isolates the PG_TRY block. Otherwise,
 *     the compiler might warn that the longjmp could clobber local variables.
 */
inline
void
AbstractionLayer::AnyType::backendGetTypeIDForFunctionArg(uint16_t inID,
    Oid &outTypeID, bool &outIsMutable) const {
    
    madlib_assert(mContent == FunctionComposite, std::logic_error(
        "Inconsistency detected while converting from PostgreSQL to C++ types."));
    
    bool exceptionOccurred = false;

    PG_TRY(); {
        outTypeID = get_fn_expr_argtype(fcinfo->flinfo, inID);    

        // If we are called as an aggregate function, the first argument is the
        // transition state. In that case, we are free to modify the data.
        // In fact, for performance reasons, we *should* even do all modifications
        // in-place. In all other cases, directly modifying memory is dangerous.
        // See warning at:
        // http://www.postgresql.org/docs/current/static/xfunc-c.html#XFUNC-C-BASETYPE
        outIsMutable = (inID == 0 && AggCheckCallContext(fcinfo, NULL));
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();
    
    if (exceptionOccurred)
        throw PGException();
}

/**
 * @brief Internal function for retrieving the type ID and datum for an element
 *     of a native composite type
 *
 * @internal
 *     Haveing this as separate function isolates the PG_TRY block. Otherwise,
 *     the compiler might warn that the longjmp could clobber local variables.
 */
inline
void
AbstractionLayer::AnyType::backendGetTypeIDAndDatumForTupleElement(
    uint16_t inID, Oid &outTypeID, Datum &outDatum) const {

    madlib_assert(mContent == NativeComposite, std::logic_error(
        "Inconsistency detected while converting from PostgreSQL to C++ types."));
    
    bool exceptionOccurred = false;
    Oid tupType;
    int32 tupTypmod;
    TupleDesc tupDesc;
    bool isNull = false;
    
    PG_TRY(); {
        tupType = HeapTupleHeaderGetTypeId(mTupleHeader);
        tupTypmod = HeapTupleHeaderGetTypMod(mTupleHeader);
        tupDesc = lookup_rowtype_tupdesc(tupType, tupTypmod);
        outTypeID = tupDesc->attrs[inID]->atttypid;
        ReleaseTupleDesc(tupDesc);
        outDatum = GetAttributeByNum(mTupleHeader, inID, &isNull);
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();
    
    if (exceptionOccurred)
        throw PGException();
}

/**
 * @brief Internal function for retrieving if the type is composite and, if
 *     yes, the PostgreSQL HeapTupleHeader
 *
 */
inline
void
AbstractionLayer::AnyType::backendGetIsCompositeTypeAndHeapTupleHeader(
    Oid inTypeID, Datum inDatum, bool &outIsTuple,
    HeapTupleHeader &outTupleHeader) const {

    boost::tribool isComposite = isRowTypeInCache(inTypeID);
    
    if (!isComposite) {
        outIsTuple = false;
        return;
    }

    bool exceptionOccurred = false;
    PG_TRY(); {
        if (boost::indeterminate(isComposite))
            outIsTuple = isRowTypeInCache(inTypeID, type_is_rowtype(inTypeID));
        
        if (outIsTuple)
            outTupleHeader = DatumGetHeapTupleHeader(inDatum);
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();

    if (exceptionOccurred)
        throw PGException();
}

/**
 * @brief Internal function for retrieving if the type is composite and, if
 *     yes, the PostgreSQL TupleDesc
 */
inline
void
AbstractionLayer::AnyType::backendGetIsCompositeTypeAndTupleHandle(
    Oid inTargetTypeID, boost::tribool &ioTargetIsComposite,
    TupleHandle &outTupleHandle) const {
    
    bool exceptionOccurred = false;

    PG_TRY(); {
        if (boost::indeterminate(ioTargetIsComposite))
            ioTargetIsComposite = isRowTypeInCache(inTargetTypeID,
                type_is_rowtype(inTargetTypeID));
        
        if (ioTargetIsComposite) {
            // Don't ereport errors. We set typmod < 0, and this should
            // not cause an error because compound types in another
            // compound can never be transient. (I think)

            outTupleHandle.desc = lookup_rowtype_tupdesc_noerror(
                inTargetTypeID, -1, true);
            outTupleHandle.shouldReleaseDesc = true;
        }
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();

    if (exceptionOccurred)
        throw PGException();
}

/**
 * @brief Return the n-th element from a composite value
 */
inline
AbstractionLayer::AnyType
AbstractionLayer::AnyType::operator[](uint16_t inID) const {
    consistencyCheck();

    if (isNull())
        throw std::invalid_argument("Unexpected Null value in function "
            "argument.");
    if (!isComposite())
        throw std::invalid_argument("Invalid type conversion requested. "
            "Expected composite type but got simple type.");
    
    if (mContent == ReturnComposite)
        return mChildren[inID];

    Oid typeID = 0;
    bool isMutable = false;
    Datum datum = 0;
    bool isTuple = false;
    HeapTupleHeader pgTuple = NULL;

    try {
        if (mContent == FunctionComposite) {
            if (inID >= size_t(PG_NARGS()))
                throw std::out_of_range("Access behind end of argument list");
            
            if (PG_ARGISNULL(inID))
                return AnyType();
                    
            backendGetTypeIDForFunctionArg(inID, typeID, isMutable);
            datum = PG_GETARG_DATUM(inID);
        } else if (mContent == NativeComposite)
            backendGetTypeIDAndDatumForTupleElement(inID, typeID, datum);
        
        if (typeID == InvalidOid)
            throw std::invalid_argument("Backend returned invalid type ID.");
        
        backendGetIsCompositeTypeAndHeapTupleHeader(typeID, datum, isTuple,
            pgTuple);
    } catch (PGException &e) {
        throw std::invalid_argument("An exception occurred while "
            "gathering inormation about PostgreSQL function arguments.");
    }

    return isTuple ?
        AnyType(pgTuple, datum, typeID) :
        AnyType(PG_GETARG_DATUM(inID), typeID, isMutable);
}

/**
 * @brief Add an element to a composite value, for returning to the backend
 */
inline
AbstractionLayer::AnyType&
AbstractionLayer::AnyType::operator<<(const AnyType &inValue) {
    consistencyCheck();

    madlib_assert(mContent == Null || mContent == ReturnComposite,
        std::logic_error("Internal inconsistency while creating composite "
            "return value."));
    
    mContent = ReturnComposite;
    mChildren.push_back(inValue);    
    return *this;
}

inline
AbstractionLayer::AnyType::TupleHandle::~TupleHandle() {
    if (shouldReleaseDesc)
        ReleaseTupleDesc(desc);
}

/**
 * @brief Get or set in our own cache whether a type ID is a composite type
 *
 * In order to minimize calls into the backend, we keep our own cache of whether
 * a type ID is a tuple type or not.
 * FIXME: In theory, this information can change during the lifetime of the hash
 * table if the user deletes a type and then creates another type that happens
 * to get the old OID.
 *
 * @param inTypeID The type ID
 * @param inIsRowType If indeterminate, just retrieve value from cache.
 *     Otherwise, update cache with the supplied value.
 * @returns Whether \c inTypeID is a composite type, indeterminate if no
 *     information is available in the cache.
 *
 * We store type information in an unordered_map. Memory for this hash
 * table is taken from malloc because the default memory context (the
 * current function) is too short-lived.
 */
inline
boost::tribool
AbstractionLayer::AnyType::isRowTypeInCache(Oid inTypeID,
    boost::tribool inIsRowType) const {
    
    typedef std::unordered_map<
        Oid, bool, std::hash<Oid>, std::equal_to<Oid>,
        utils::MallocAllocator<std::pair<const Oid, bool> > > Oid2BoolHashMap;

    static Oid2BoolHashMap sIsTuple(32);
    
    if (inIsRowType != boost::indeterminate)
        return sIsTuple[inTypeID] = inIsRowType;
    else if (sIsTuple.find(inTypeID) != sIsTuple.end())
        return sIsTuple[inTypeID];
    else
        return boost::indeterminate;
}

/**
 * @brief Convert the current object to a PostbreSQL Datum
 *
 * If the current object is Null, we still return <tt>Datum(0)</tt>, i.e., we
 * return a valid Datum. It is the responsibilty of the caller to separately
 * call isNull().
 *
 * @param inFCInfo The PostgreSQL FunctionCallInfo that was passed to the UDF.
 *     This is necessary for verifying that the top-level AnyType has the
 *     correct type.
 */
inline
Datum
AbstractionLayer::AnyType::getAsDatum(const FunctionCallInfo inFCInfo) {
    consistencyCheck();

    Oid targetTypeID;
    TupleDesc tupleDesc;
    TypeFuncClass funcClass;
    bool exceptionOccurred = false;

    PG_TRY(); {
        // FIXME: get_call_result_type is tagged as expensive in funcapi.c
        // It seems not to be necessary to release the tupleDesc
        // E.g., in plython.c ReleaseTupleDesc() is not called
        funcClass = get_call_result_type(inFCInfo, &targetTypeID, &tupleDesc);
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();
    
    if (exceptionOccurred)
        throw std::invalid_argument("An exception occurred while "
            "gathering inormation about the PostgreSQL return type.");

    bool targetIsComposite = (funcClass == TYPEFUNC_COMPOSITE);
    if (targetIsComposite && !isComposite())
        throw std::logic_error("Invalid type conversion requested. "
            "Simple type supplied but PostgreSQL expects composite type.");

    if (!targetIsComposite && isComposite())
        throw std::logic_error("Invalid type conversion requested. "
            "Composite type supplied but PostgreSQL expects simple type.");

    // tupleDesc can be NULL if the return type is not composite
    return getAsDatum(targetTypeID, isComposite(), tupleDesc);
}

/**
 * @brief Convert the current object to a PostbreSQL Datum
 *
 * @see getAsDatum(const FunctionCallInfo)
 */
inline
Datum
AbstractionLayer::AnyType::getAsDatum(Oid inTargetTypeID,
    boost::tribool inTargetIsComposite, TupleDesc inTargetTupleDesc) const {
    
    consistencyCheck();

    // The default value to return in case of Null is 0. Note, however, that
    // 0 can also be a perfectly valid (non-null) Datum. It is the caller's
    // responsibility to call isNull() separately.
    if (isNull())
        return 0;

    try {
        bool exceptionOccurred = false;
        TupleHandle tupleHandle(inTargetTupleDesc);
        
        if (boost::indeterminate(inTargetIsComposite)) {
            inTargetIsComposite = isRowTypeInCache(inTargetTypeID);
            backendGetIsCompositeTypeAndTupleHandle(inTargetTypeID,
                inTargetIsComposite, tupleHandle);
        }
        
        if (inTargetIsComposite && !isComposite())
            throw std::logic_error("Invalid type conversion requested. "
                "Simple type supplied but PostgreSQL expects composite type.");

        if (!inTargetIsComposite && isComposite())
            throw std::logic_error("Invalid type conversion requested. "
                "Composite type supplied but PostgreSQL expects simple type.");
        
        madlib_assert(inTargetIsComposite == (tupleHandle.desc != NULL),
            MADLIB_DEFAULT_EXCEPTION);
        
        if (inTargetIsComposite) {
            std::vector<Datum> values;
            std::vector<char> nulls;

            for (uint16_t pos = 0; pos < mChildren.size(); ++pos) {
                Oid targetTypeID = tupleHandle.desc->attrs[pos]->atttypid;
                                    
                values.push_back(mChildren[pos].getAsDatum(targetTypeID));
                nulls.push_back(mChildren[pos].isNull());
            }
            
            Datum returnValue;
            PG_TRY(); {
                HeapTuple heapTuple = heap_form_tuple(tupleHandle.desc,
                    &values[0], reinterpret_cast<bool*>(&nulls[0]));
                
                returnValue = HeapTupleGetDatum(heapTuple);
            } PG_CATCH(); {
                exceptionOccurred = true;
            } PG_END_TRY();
            
            if (exceptionOccurred)
                throw PGException();
            
            return returnValue;
        }
    } catch (PGException &e) {
        throw std::invalid_argument("An exception occurred while "
            "gathering inormation about the PostgreSQL return type.");
    }
        
    if (inTargetTypeID != mTypeID)
        throw std::invalid_argument("Invalid type conversion requested. "
            "C++ type and PostgreSQL return type do not match.");
    
    return mDatum;
}
