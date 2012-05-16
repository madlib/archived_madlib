/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyType_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by dbconnector.hpp
#ifdef MADLIB_DBCONNECTOR_HPP

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

/**
 * @brief Template constructor (will \b not be used as copy constructor)
 *
 * This constructor will be invoked when initializing an AnyType object with
 * any scalar value (including arrays, but excluding composite types). This will
 * typically only happen for preparing the return value of a user-defined
 * function.
 * This constructor immediately converts the object into a PostgreSQL Datum
 * using the TypeTraits class. If memory has to be retained, it has to be done
 * there.
 */
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

/**
 * @brief Default constructor, initializes AnyType object as Null
 *
 * This constructor initializes the object as Null. It must also be used for
 * building a composite type. After construction, use operator<<() to append
 * values to the composite object.
 */
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
 * @brief Verify consistency of AnyType object. Throw exception if not.
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

/**
 * @brief Convert object to the type specified as template argument
 *
 * @tparam T Type to convert object to
 */
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
    
    bool needMutableClone = (TypeTraits<T>::isMutable && !mIsMutable);
    
    return TypeTraits<T>::toCXXType(mDatum, needMutableClone);
}

/**
 * @brief Return if object is Null
 */
inline
bool
AbstractionLayer::AnyType::isNull() const {
    return mContent == Null;
}

/**
 * @brief Return if object is of composite type (also called row type or
 *     user-defined type)
 */
inline
bool
AbstractionLayer::AnyType::isComposite() const {
    return mContent == FunctionComposite || mContent == NativeComposite ||
        mContent == ReturnComposite;
}

/**
 * @brief Return the number of fields in a composite value.
 *
 * @returns The number of fields in a composite value. In the case of a scalar
 *     value, return 1. If the content is NULL, return 0.
 */
inline
uint16_t
AnyType::numFields() const {
    switch (mContent) {
        case Null: return 0;
        case Scalar: return 1;
        case ReturnComposite: return mChildren.size();
        case FunctionComposite: return PG_NARGS();
        case NativeComposite: return HeapTupleHeaderGetNatts(mTupleHeader);
        default:
            // This should never happen
            throw std::logic_error("Unhandled case in AnyType::numFields().");
    }
}

/**
 * @brief Internal function for determining the type of a function argument
 *
 * @param inID Number of function argument
 * @param[out] outTypeID PostgreSQL OID of the function argument's type
 * @param[out] outIsMutable True if the data structure of this function argument
 *     can be safely modified. For objects passed by reference (like arrays)
 *     this is only true when passed as the first argument of a transition
 *     function.
 *
 * @internal
 *     Having this as separate function isolates the PG_TRY block. Otherwise,
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
 * @param inID Number of function argument
 * @param[out] outTypeID PostgreSQL OID of the function argument's type
 * @param[out] outDatum PostgreSQL Datum for the function argument
 *
 * @internal
 *     Having this as separate function isolates the PG_TRY block. Otherwise,
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
 * @param inTypeID PostgreSQL OID for the type
 * @param inDatum PostgreSQL Datum
 * @param[out] outIsTuple True if the type is a composite type
 * @param[out] outTupleHeader A PostgreSQL HeapTupleHeader that will be updated
 *     if type is composite
 *
 * @internal
 *     Having this as separate function isolates the PG_TRY block. Otherwise,
 *     the compiler might warn that the longjmp could clobber local variables. 
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
 *
 * @param inTargetTypeID PostgreSQL OID for the type
 * @param[in,out] ioTargetIsComposite On input, whether the type is composite.
 *     \c indeterminate if unknown. On return, the updated boolean value.
 * @param[out] outTupleHandle TupleHandle that will be updated if type is
 *     composite
 *
 * @internal
 *     Having this as separate function isolates the PG_TRY block. Otherwise,
 *     the compiler might warn that the longjmp could clobber local variables.
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
 *
 * To the user, AnyType is a fully recursive type: Each AnyType object can be a
 * composite object and be composed of a number of other AnyType objects.
 * On top of the C++ abstraction layer, function have a single-top level
 * AnyType object as parameter.
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
            "gathering information about PostgreSQL function arguments.");
    }

    return isTuple ?
        AnyType(pgTuple, datum, typeID) :
        AnyType(datum, typeID, isMutable);
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
 * @brief Return a PostgreSQL Datum representing the current object
 *
 * The only *conversion* taking place in this function is *combining* Datums
 * into a tuple. At this place, we do not have to worry any more about retaining
 * memory.
 *
 * @param inTargetTypeID PostgreSQL OID of the target type to convert to
 * @param inTargetIsComposite Whether the target type is composite.
 *     \c indeterminate if unknown.
 * @param inTargetTupleDesc If target type is known to be composite, then
 *     (optionally) the PostgreSQL TupleDesc. NULL is always a valid argument.
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
            throw std::runtime_error("Invalid type conversion requested. "
                "Simple type supplied but PostgreSQL expects composite type.");

        if (!inTargetIsComposite && isComposite())
            throw std::runtime_error("Invalid type conversion requested. "
                "Composite type supplied but PostgreSQL expects simple type.");
        
        madlib_assert(inTargetIsComposite == (tupleHandle.desc != NULL),
            MADLIB_DEFAULT_EXCEPTION);
        
        if (inTargetIsComposite) {
            if (static_cast<size_t>(tupleHandle.desc->natts) < mChildren.size())
                throw std::runtime_error("Invalid type conversion requested. "
                    "Internal composite type has more elements than PostgreSQL "
                    "composite type.");

            std::vector<Datum> values;
            std::vector<char> nulls;

            for (uint16_t pos = 0; pos < mChildren.size(); ++pos) {
                Oid targetTypeID = tupleHandle.desc->attrs[pos]->atttypid;
                                    
                values.push_back(mChildren[pos].getAsDatum(targetTypeID));
                nulls.push_back(mChildren[pos].isNull());
            }
            // All elements that have not been initialized will be set to Null
            for (uint16_t pos = mChildren.size();
                pos < static_cast<size_t>(tupleHandle.desc->natts);
                ++pos) {
                
                values.push_back(Datum(0));
                nulls.push_back(true);
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

#endif // MADLIB_DBCONNECTOR_HPP (workaround for Doxygen)
