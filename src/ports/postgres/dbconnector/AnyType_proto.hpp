/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyType_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_ANYTYPE_PROTO_HPP
#define MADLIB_POSTGRES_ANYTYPE_PROTO_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

struct SystemInformation;

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
 * - A return composite value, which is a vector of AnyType objects
 */
class AnyType {
public:
    AnyType();
    template <typename T> AnyType(const T& inValue);
    template <typename T> T getAs() const;
    AnyType operator[](uint16_t inID) const;
    uint16_t numFields() const;
    bool isNull() const;
    bool isComposite() const;
    AnyType &operator<<(const AnyType& inValue);

protected:
    // UDF and FunctionHandle access getAsDatum(), which is not part of the
    // public API
    friend class UDF;
    friend class FunctionHandle;

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
    
    AnyType(FunctionCallInfo inFnCallInfo);
    AnyType(SystemInformation* inSysInfo, HeapTupleHeader inTuple,
        Datum inDatum, Oid inTypeID);
    AnyType(SystemInformation* inSysInfo, Datum inDatum, Oid inTypeID,
        bool inIsMutable);
    void consistencyCheck() const;
    Datum getAsDatum(FunctionCallInfo inFCInfo,
        Oid inTargetTypeID = InvalidOid) const;

    Content mContent;
    Datum mDatum;
    FunctionCallInfo fcinfo;
    SystemInformation* mSysInfo;
    HeapTupleHeader mTupleHeader;
    std::vector<AnyType> mChildren;
    Oid mTypeID;
    const char* mTypeName;
    bool mIsMutable;
};

AnyType Null();

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_ANYTYPE_PROTO_HPP)
