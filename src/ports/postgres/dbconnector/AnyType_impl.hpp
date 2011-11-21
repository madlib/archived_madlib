/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyType.hpp
 *
 * @brief Convert PostgreSQL types into C++ types
 *
 *//* ----------------------------------------------------------------------- */

inline
AbstractionLayer::AnyType::AnyType(FunctionCallInfo inFnCallInfo)
  : mDatum(0),
    fcinfo(inFnCallInfo),
    mTupleHeader(NULL),
    mTypeID(InvalidOid),
    mIsMutable(false)
    { }

inline    
AbstractionLayer::AnyType::AnyType(HeapTupleHeader inTuple, Oid inTypeID)
  : mDatum(0),
    fcinfo(NULL),
    mTupleHeader(inTuple),
    mTypeID(inTypeID),
    mIsMutable(false)
    { }

inline
AbstractionLayer::AnyType::AnyType(Datum inDatum, Oid inTypeID, bool inIsMutable)
  : mDatum(inDatum),
    fcinfo(NULL),
    mTupleHeader(NULL),
    mTypeID(inTypeID),
    mIsMutable(inIsMutable)
    { }

template <typename T>
inline
AbstractionLayer::AnyType::AnyType(const T &inValue)
  : mDatum(TypeBridge<T>::toDatum(inValue)),
    fcinfo(NULL),
    mTupleHeader(NULL),
    mTypeID(TypeBridge<T>::oid),
    mIsMutable(false)
    { }

inline
AbstractionLayer::AnyType::AnyType()
  : mDatum(0),
    fcinfo(NULL),
    mTupleHeader(NULL),
    mTypeID(InvalidOid),
    mIsMutable(false)
    { }

inline
void
AbstractionLayer::AnyType::consistencyCheck() const {
    madlib_assert( (mDatum != 0) + (fcinfo != NULL) + (mTupleHeader != NULL) +
        (!mChildren.empty()) <= 1,
        std::logic_error("Inconsistency detected while converting between "
            "PostgreSQL and C++ types.") );
}

template <dbal::TypeClass TC>
void
AbstractionLayer::AnyType::typeClassCheck() const {
    if (isNull())
        throw std::invalid_argument("Conversion of Null value requested.");

    consistencyCheck();

    if (TC != dbal::SimpleType && !mDatum)
        throw std::invalid_argument("Invalid type conversion requested. "
            "Expected simple or array type but got composite type.");
    
    if (TC == dbal::CompositeType && !mTupleHeader)
        throw std::invalid_argument("Invalid type conversion requested. "
            "Expected composite type but got simple type.");
}

inline
void
AbstractionLayer::AnyType::throwWrongType() const {
    throw std::invalid_argument(
        "Expected function argument to be of different type");
}

template <typename T>
T AbstractionLayer::AnyType::getAs() const {
    typeClassCheck<dbal::SimpleType>();
    switch (mTypeID) {
        case TypeBridge<T>::oid:
            return TypeBridge<T>::toCXXType(mDatum);
        default: throwWrongType(); return 0;
    }
}

inline
bool
AbstractionLayer::AnyType::isNull() const {
    return mDatum == 0 && fcinfo == NULL && mTupleHeader == NULL &&
        mChildren.empty();
}

inline
bool
AbstractionLayer::AnyType::isComposite() const {
    return mTupleHeader != NULL || !mChildren.empty();
}

/**
 * @internal
 *     We have this as a separate function in order to isolate the PG_TRY
 *     block. Otherwise, there compiler might warn that the longjmp could
 *     clobber local variables.
 */
inline
void
AbstractionLayer::AnyType::getFnExprArgType(FunctionCallInfo fcinfo,
    uint16_t inID, Oid &outTypeID, bool &outIsMutable) const {
    
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

inline
void
AbstractionLayer::AnyType::getTupleDatum(uint16_t inID, Oid &outTypeID,
    Datum &outDatum) const {
    
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

inline
void
AbstractionLayer::AnyType::getIsRowTypeAndTupleHeader(Oid inTypeID,
    Datum inDatum, bool &outIsTuple, HeapTupleHeader &outTupleHeader) const {

    bool exceptionOccurred = false;
    PG_TRY(); {
        outIsTuple = type_is_rowtype(inTypeID);
        
        if (outIsTuple)
            outTupleHeader = DatumGetHeapTupleHeader(inDatum);
    } PG_CATCH(); {
        exceptionOccurred = true;
    } PG_END_TRY();

    if (exceptionOccurred)
        throw PGException();
}

inline
AbstractionLayer::AnyType
AbstractionLayer::AnyType::operator[](uint16_t inID) const {
    if (isNull())
        throw std::invalid_argument("Null value where not expected.");

    consistencyCheck();

    if (mDatum != 0)
        throw std::invalid_argument("Invalid type conversion requested. "
            "Expected composite type but got simple type.");
    
    if (!mChildren.empty())
        return mChildren[inID];

    Oid typeID = 0;
    bool isMutable = false;
    Datum datum = 0;
    bool isTuple = false;
    HeapTupleHeader pgTuple = NULL;

    try {
        if (fcinfo != NULL) {
            if (inID >= size_t(PG_NARGS()))
                throw std::out_of_range("Access behind end of argument list");
            
            if (PG_ARGISNULL(inID))
                return AnyType();
                    
            getFnExprArgType(fcinfo, inID, typeID, isMutable);
            datum = PG_GETARG_DATUM(inID);
        } else if (mTupleHeader != NULL)
            getTupleDatum(inID, typeID, datum);
        
        if (typeID == InvalidOid)
            throw std::invalid_argument("Cannot determine function argument type.");
        
        getIsRowTypeAndTupleHeader(typeID, datum, isTuple, pgTuple);
    } catch (PGException &e) {
        throw std::invalid_argument("An exception occurred while "
            "gathering inormation about PostgreSQL function arguments.");
    }

    return isTuple ?
        AnyType(pgTuple, typeID) :
        AnyType(PG_GETARG_DATUM(inID), typeID, isMutable);
}

inline
AbstractionLayer::AnyType&
AbstractionLayer::AnyType::operator<<(const AnyType &inValue) {
    madlib_assert(mDatum == 0 && fcinfo == NULL && mTupleHeader == NULL,
        std::logic_error("Invalid composite type creation."));
    
    mChildren.push_back(inValue);    
    return *this;
}

inline
Datum
AbstractionLayer::AnyType::getAsDatum(const FunctionCallInfo inFCInfo) {
    consistencyCheck();

    Oid targetTypeID;
    TupleDesc tupleDesc;
    TypeFuncClass funcClass;
    bool exceptionOccurred = false;

    PG_TRY(); {
        // FIXME: (Or Note:) get_call_result_type is tagged as expensive in funcapi.c
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

    return getAsDatum(targetTypeID, isComposite(), tupleDesc);
}

inline
Datum
AbstractionLayer::AnyType::getAsDatum(Oid inTargetTypeID, boost::tribool inTargetIsComposite,
    TupleDesc inTargetTupleDesc) {
    
    if (isNull())
        return 0;

    consistencyCheck();

    try {
        bool exceptionOccurred = false;
        
        struct TupleHandle {
            TupleHandle(TupleDesc inTargetTupleDesc)
              : desc(inTargetTupleDesc), shouldReleaseDesc(false) { }
            
            ~TupleHandle() {
                if (shouldReleaseDesc)
                    ReleaseTupleDesc(desc);
            }
            
            TupleDesc desc;
            bool shouldReleaseDesc;
        } tupleHandle(inTargetTupleDesc);
        
        if (inTargetIsComposite == boost::indeterminate) {
            PG_TRY(); {
                inTargetIsComposite = type_is_rowtype(inTargetTypeID);
                
                if (inTargetIsComposite) {
                    // Don't ereport errors. We set typmod < 0, and this should
                    // not cause an error because compound types in another
                    // compound can never be transient. (I think)

                    tupleHandle.desc = lookup_rowtype_tupdesc_noerror(
                        inTargetTypeID, -1, true);
                    tupleHandle.shouldReleaseDesc = true;
                }
            } PG_CATCH(); {
                exceptionOccurred = true;
            } PG_END_TRY();

            if (exceptionOccurred)
                throw PGException();
        }
        
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
    
    madlib_assert(mDatum != 0, MADLIB_DEFAULT_EXCEPTION);
    
    if (inTargetTypeID != mTypeID)
        throw std::invalid_argument("Invalid type conversion requested. "
            "C++ type and PostgreSQL return type do not match.");
    
    return mDatum;
}
