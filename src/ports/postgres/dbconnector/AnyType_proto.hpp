/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyType_proto.hpp
 *
 * @brief Convert PostgreSQL types into C++ types
 *
 *//* ----------------------------------------------------------------------- */

class AbstractionLayer::AnyType {
public:
    AnyType();
    template <typename T> AnyType(const T &inValue);
    template <typename T> T getAs() const;
    AbstractionLayer::AnyType operator[](uint16_t inID) const;
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

    AnyType(HeapTupleHeader inTuple, Oid inTypeID);
    AnyType(Datum inDatum, Oid inTypeID, bool inIsMutable);
    void consistencyCheck() const;
    Datum getAsDatum(const FunctionCallInfo inFCInfo);
    template <dbal::TypeClass TC> void typeClassCheck() const;
    void throwWrongType() const;
    Datum getAsDatum(Oid inTargetTypeID,
        boost::tribool inIsComposite = boost::indeterminate,
        TupleDesc inTupleDesc = NULL);
    void getFnExprArgType(FunctionCallInfo fcinfo, uint16_t inID,
        Oid &outTypeID, bool &outIsMutable) const;
    void getTupleDatum(uint16_t inID, Oid &outTypeID, Datum &outDatum) const;
    void getIsRowTypeAndTupleHeader(Oid inTypeID, Datum inDatum,
        bool &outIsTuple, HeapTupleHeader &outTupleHeader) const;

    Datum mDatum;
    FunctionCallInfo fcinfo;
    HeapTupleHeader mTupleHeader;
    std::vector<AnyType> mChildren;
    Oid mTypeID;
    bool mIsMutable;
};
