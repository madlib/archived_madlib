/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyType_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by dbconnector.hpp
#ifdef MADLIB_DBCONNECTOR_HPP

/**
 * @brief Proxy for PostgreSQL objects
 *
 * AnyType objects are used by user-defined code to both retrieve and return
 * values from the backend.
 *
 * The content of an AnyType object is specified by mContent. It can be:
 * - Null
 * - A scalar value, which is just a PostgreSQL Datum
 * - A function composite value, which is a virtual composite value consisting
 *   of all function arguments
 * - A native composite value, which is a PostgreSQL HeapTupleHeader
 * - A return composite value, which is a vector of PostgreSQL datums
 */
class AbstractionLayer::AnyType {
public:
    AnyType();
    template <typename T> AnyType(const T &inValue);
    template <typename T> T getAs() const;
    AbstractionLayer::AnyType operator[](uint16_t inID) const;
    uint16_t numFields() const;
    bool isNull() const;
    bool isComposite() const;
    bool isArray() const;
    AbstractionLayer::AnyType &operator<<(const AbstractionLayer::AnyType &inValue);

    // Methods not part of the AnyType interface
    AnyType(FunctionCallInfo inFnCallInfo);

protected:
    // UDF cannot access a protected member (getAsDatum) of a nested class
    // (AnyType) of its super class (AbstractionLayer)
    friend class UDF;

    /**
     * @brief Type of the value of the current AnyType object
     */
    enum Content {
        Null,
        Scalar,
        FunctionComposite,
        NativeComposite,
        ReturnComposite
    };

    /**
     * @brief RAII encapsulation of a TupleDesc. Calls ReleaseTupleDesc() when
     *     necessary.
     */
    struct TupleHandle {
        TupleHandle(TupleDesc inTargetTupleDesc)
          : desc(inTargetTupleDesc), shouldReleaseDesc(false) { }
        ~TupleHandle();
        
        TupleDesc desc;
        bool shouldReleaseDesc;
    };

    AnyType(HeapTupleHeader inTuple, Datum inDatum, Oid inTypeID);
    AnyType(Datum inDatum, Oid inTypeID, bool inIsMutable);
    void consistencyCheck() const;
    
    void backendGetTypeIDForFunctionArg(uint16_t inID, Oid &outTypeID,
        bool &outIsMutable) const;
    void backendGetTypeIDAndDatumForTupleElement(uint16_t inID, Oid &outTypeID,
        Datum &outDatum) const;
    void backendGetIsCompositeTypeAndHeapTupleHeader(Oid inTypeID,
        Datum inDatum, bool &outIsTuple, HeapTupleHeader &outTupleHeader) const;
    void backendGetIsCompositeTypeAndTupleHandle(Oid inTargetTypeID,
        boost::tribool &ioTargetIsComposite, TupleHandle &outTupleHandle) const;
    
    boost::tribool isRowTypeInCache(Oid inTypeID,
        boost::tribool inIsRowType = boost::indeterminate) const;
    
    Datum getAsDatum(const FunctionCallInfo inFCInfo);
    Datum getAsDatum(Oid inTargetTypeID,
        boost::tribool inIsComposite = boost::indeterminate,
        TupleDesc inTupleDesc = NULL) const;

    Content mContent;
    Datum mDatum;
    FunctionCallInfo fcinfo;
    HeapTupleHeader mTupleHeader;
    std::vector<AnyType> mChildren;
    Oid mTypeID;
    bool mIsMutable;
};

#endif // MADLIB_DBCONNECTOR_HPP (workaround for Doxygen)
