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
    template <typename T> AnyType(const T& inValue,
        bool inForceLazyConversionToDatum = false);
    template <typename T> T getAs() const;
    AnyType operator[](uint16_t inID) const;
    uint16_t numFields() const;
    bool isNull() const;
    bool isComposite() const;
    AnyType& operator<<(const AnyType& inValue);

    // FIXME: A temporary workaround for UDF to get/set user_fctx, a better
    // solution is desired which complies with the design principles of DB
    // abstraction layer.
    // For some analytic algorithms (e.g. LDA), a stateful function would be
    // very useful which allows to carry some states accross invocations within
    // a query. Alternatively, one can achieve similar functionaltiy using a
    // windowed aggregator, but this may suffer from severe performance
    // degradation.  
    void * getUserFuncContext();
    void setUserFuncContext(void * user_fctx);
    MemoryContext getCacheMemoryContext();
protected:
    /**
     * @brief RAII class to temporarily change \c sLazyConversionToDatum
     */
    class LazyConversionToDatumOverride {
    public:
        LazyConversionToDatumOverride(bool inLazyConversionToDatum);
        ~LazyConversionToDatumOverride();
    protected:
        bool mOriginalValue;
    };

    static bool sLazyConversionToDatum;
    
    static bool lazyConversionToDatum();

    // UDF and FunctionHandle access getAsDatum(), which is not part of the
    // public API
    friend class UDF;
    friend class FunctionHandle;

    /**
     * @brief Type of the value of the current AnyType object
     */
    enum ContentType {
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

    class Placeholder;

    ContentType mContentType;
    boost::any mContent;
    std::function<Datum()> mToDatumFn;
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

/**
 * @brief Cast that extract the proper type from AnyType but leaves other types
 *     unaffected
 *
 * Sometimes it is desirable to write generic code that works on both an
 * \c AnyType object as well as a value with a concrete type. For instance, a
 * \c FunctionHandle always returns an \c AnyType object. In generatic code,
 * however, a \c FunctionHandle might as well be replaced by just call a
 * "normal" functor or a C++ function pointer, both of which typically return
 * concrete types (e.g., double).
 *
 * In generic code, we could write <tt>AnyType_cast<double>(func())</tt> so that
 * if the template parameter \c Function is \c AnyType, we have an explicit
 * conversion to \c double, and if \c Function is just a function pointer, the
 * return value of <tt>func()</tt> passes unchanged.
 */
template <class T>
inline
T
AnyType_cast(const AnyType& inValue) {
    return inValue.getAs<T>();
}

template <class T>
inline
const T&
AnyType_cast(const T& inValue) {
    return inValue;
}

template <class T>
inline
T&
AnyType_cast(T& inValue) {
    return inValue;
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_ANYTYPE_PROTO_HPP)
