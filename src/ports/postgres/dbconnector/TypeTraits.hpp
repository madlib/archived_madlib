/* ----------------------------------------------------------------------- *//**
 *
 * @file TypeTraits.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_TYPETRAITS_HPP
#define MADLIB_POSTGRES_TYPETRAITS_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

template <typename T>
struct TypeTraits;

template <class T, class U>
class convertTo {
public:
    convertTo(const T& inOrig) : mOrig(inOrig) { }
    
    operator U() {
        if (std::numeric_limits<T>::is_signed
            && !std::numeric_limits<U>::is_signed
            && utils::isNegative(mOrig)) {
            
            std::stringstream errorMsg;
            errorMsg << "Invalid value conversion. Expected unsigned value but "
                "got " << mOrig << ".";
            throw std::invalid_argument(errorMsg.str());
        } else if (
            (std::numeric_limits<T>::digits > std::numeric_limits<U>::digits
                ||  (!std::numeric_limits<T>::is_signed
                    && std::numeric_limits<U>::is_signed))
            &&  mOrig > static_cast<T>(std::numeric_limits<U>::max())) {
            
            std::stringstream errorMsg;
            errorMsg << "Invalid value conversion. Cannot represent "
                << mOrig << "in target type (" << typeid(T).name() << ").";
            throw std::invalid_argument(errorMsg.str());
        }
        return static_cast<U>(mOrig);
    }
private:
    const T& mOrig;
};
    
/*
 * The type OID (_oid) can be set to InvalidOid if it should not be used for
 * type verification. This is the case for types that are not built-in and for
 * which no fixed OID is known at compile time.
 * The type name (_type_name) can be NULL if it should not be used for type
 * verification. This is the case for built-in types, for which only the type
 * OID should be verified, but not the type name.
 * Mutable means in this context that the value of a variable can be changed and
 * that it would change the value for the backend
 */
#define DECLARE_CXX_TYPE_EXT( \
        _oid, \
        _type_name, \
        _type, \
        _typeClass, \
        _isMutable, \
        _convertToPG, \
        _convertToCXX, \
        _convertToSysInfo \
    ) \
    template <> \
    struct TypeTraits<_type > { \
        enum { oid = _oid }; \
        enum { typeClass = _typeClass }; \
        enum { isMutable = _isMutable }; \
        static inline const char* typeName() { \
            static char const* TYPE_NAME = _type_name; \
            return TYPE_NAME; \
        }; \
        static inline Datum toDatum(const _type& value) { \
            return _convertToPG; \
        }; \
        static inline SystemInformation* toSysInfo(const _type& value) { \
            (void) value; \
            return _convertToSysInfo; \
        }; \
        static inline _type toCXXType(Datum value, bool needMutableClone, \
            SystemInformation* sysInfo) { \
            \
            (void) needMutableClone; \
            (void) sysInfo; \
            return _convertToCXX; \
        }; \
    }

// Simplified macro to declare an OID <-> C++ type mapping. The return type name
// and the SystemInformation pointer are always NULL
#define DECLARE_CXX_TYPE( \
        _oid, \
        _type, \
        _typeClass, \
        _isMutable, \
        _convertToPG, \
        _convertToCXX \
    ) \
    DECLARE_CXX_TYPE_EXT(_oid, NULL, _type, _typeClass, _isMutable, \
        _convertToPG, _convertToCXX, NULL)

DECLARE_CXX_TYPE(
    FLOAT8OID,
    double,
    dbal::SimpleType,
    dbal::Immutable,
    Float8GetDatum(value),
    DatumGetFloat8(value)
);
DECLARE_CXX_TYPE(
    FLOAT4OID,
    float,
    dbal::SimpleType,
    dbal::Immutable,
    Float4GetDatum(value),
    DatumGetFloat4(value)
);
DECLARE_CXX_TYPE(
    INT8OID,
    int64_t,
    dbal::SimpleType,
    dbal::Immutable,
    Int64GetDatum(value),
    DatumGetInt64(value)
);
DECLARE_CXX_TYPE(
    INT8OID,
    uint64_t,
    dbal::SimpleType,
    dbal::Immutable,
    Int64GetDatum((convertTo<uint64_t, int64_t>(value))),
    (convertTo<int64_t, uint64_t>(DatumGetInt64(value)))
);
DECLARE_CXX_TYPE(
    INT4OID,
    int32_t,
    dbal::SimpleType,
    dbal::Immutable,
    Int32GetDatum(value),
    DatumGetInt32(value)
);
DECLARE_CXX_TYPE(
    INT4OID,
    uint32_t,
    dbal::SimpleType,
    dbal::Immutable,
    Int32GetDatum((convertTo<uint32_t, int32_t>(value))),
    (convertTo<int32_t, uint32_t>(DatumGetInt32(value)))
);
DECLARE_CXX_TYPE(
    BOOLOID,
    bool,
    dbal::SimpleType,
    dbal::Immutable,
    BoolGetDatum(value),
    DatumGetBool(value)
);
DECLARE_CXX_TYPE_EXT(
    REGPROCOID,
    NULL,
    FunctionHandle,
    dbal::SimpleType,
    dbal::Immutable,
    ObjectIdGetDatum(value.funcID()),
    FunctionHandle(sysInfo, DatumGetObjectId(value)),
    value.getSysInfo()
);
DECLARE_CXX_TYPE(
    FLOAT8ARRAYOID,
    ArrayHandle<double>,
    dbal::ArrayType,
    dbal::Immutable,
    PointerGetDatum(value.array()),
    reinterpret_cast<ArrayType*>(madlib_DatumGetArrayTypeP(value))
);
DECLARE_CXX_TYPE(
    FLOAT8ARRAYOID,
    dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::ColumnVector>,
    dbal::ArrayType,
    dbal::Immutable,
    PointerGetDatum(value.memoryHandle().array()),
    ArrayHandle<double>(reinterpret_cast<ArrayType*>(madlib_DatumGetArrayTypeP(value)))
);
// Note: See the comment for PG_FREE_IF_COPY in fmgr.h. Essentially, when
// writing UDFs, we do not have to worry about deallocating copies of immutable
// arrays. They will simply be garbage collected.
DECLARE_CXX_TYPE(
    FLOAT8ARRAYOID,
    MutableArrayHandle<double>,
    dbal::ArrayType,
    dbal::Mutable,
    PointerGetDatum(value.array()),
    needMutableClone
      ? madlib_DatumGetArrayTypePCopy(value)
      : madlib_DatumGetArrayTypeP(value)
);
DECLARE_CXX_TYPE(
    FLOAT8ARRAYOID,
    dbal::eigen_integration::HandleMap<dbal::eigen_integration::ColumnVector>,
    dbal::ArrayType,
    dbal::Mutable,
    PointerGetDatum(value.memoryHandle().array()),
    MutableArrayHandle<double>(reinterpret_cast<ArrayType*>(
        needMutableClone
            ? madlib_DatumGetArrayTypePCopy(value)
            : madlib_DatumGetArrayTypeP(value)
    ))
);
DECLARE_CXX_TYPE_EXT(
    InvalidOid,
    "svec",
    dbal::eigen_integration::SparseColumnVector,
    dbal::SimpleType,
    dbal::Immutable,
    PointerGetDatum(SparseColumnVectorToLegacySparseVector(value)),
    LegacySparseVectorToSparseColumnVector(reinterpret_cast<SvecType*>(value)),
    NULL
);

#undef DECLARE_OID_TO_TYPE_MAPPING
#undef DECLARE_CXX_TYPE
#undef DECLARE_OID

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_TYPETRAITS_HPP)
