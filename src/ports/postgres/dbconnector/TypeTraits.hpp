/* ----------------------------------------------------------------------- *//**
 *
 * @file TypeTraits.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by dbconnector.hpp
#ifdef MADLIB_DBCONNECTOR_HPP

/*
 * Mutable means in this context that the value of a variable can be changed and
 * that it would change the value for the backend
 */
#define DECLARE_CXX_TYPE(_oid, _type, _typeClass, _isMutable, _convertToPG, _convertToCXX) \
    template <> \
    struct AbstractionLayer::TypeTraits<_type> { \
        enum { oid = _oid }; \
        enum { typeClass = _typeClass }; \
        enum { isMutable = _isMutable }; \
        static inline Datum toDatum(const _type &value) { \
            return _convertToPG; \
        }; \
        static inline _type toCXXType(Datum value, bool needMutableClone) { \
            (void) needMutableClone; \
            return _convertToCXX; \
        }; \
    }

#define DECLARE_OID(_oid, _type) \
    template <> \
    struct AbstractionLayer::TypeForOid<_oid> { \
        typedef _type type; \
    }

#define DECLARE_OID_TO_TYPE_MAPPING(_oid, _type, _typeClass, _isMutable, _convertToPG, _convertToCXX) \
    DECLARE_CXX_TYPE(_oid, _type, _typeClass, _isMutable, _convertToPG, _convertToCXX); \
    DECLARE_OID(_oid, _type)

DECLARE_OID_TO_TYPE_MAPPING(
    FLOAT8OID,
    double,
    dbal::SimpleType,
    dbal::Immutable,
    Float8GetDatum(value),
    DatumGetFloat8(value)
);
DECLARE_OID_TO_TYPE_MAPPING(
    FLOAT4OID,
    float,
    dbal::SimpleType,
    dbal::Immutable,
    Float4GetDatum(value),
    DatumGetFloat4(value)
);
DECLARE_OID_TO_TYPE_MAPPING(
    INT8OID,
    int64_t,
    dbal::SimpleType,
    dbal::Immutable,
    Int64GetDatum(value),
    DatumGetInt64(value)
);
DECLARE_OID_TO_TYPE_MAPPING(
    INT4OID,
    int32_t,
    dbal::SimpleType,
    dbal::Immutable,
    Int32GetDatum(value),
    DatumGetInt32(value)
);
DECLARE_OID_TO_TYPE_MAPPING(
    BOOLOID,
    bool,
    dbal::SimpleType,
    dbal::Immutable,
    BoolGetDatum(value),
    DatumGetBool(value)
);
DECLARE_OID_TO_TYPE_MAPPING(
    FLOAT8ARRAYOID,
    AbstractionLayer::ArrayHandle<double>,
    dbal::ArrayType,
    dbal::Immutable,
    PointerGetDatum(value.array()),
    reinterpret_cast<ArrayType*>(DatumGetArrayTypeP(value))
);
// Note: See the comment for PG_FREE_IF_COPY in fmgr.h. Essentially, when
// writing UDFs, we do not have to worry about deallocating copies of immutable
// arrays. They will simply be garbage collected.
DECLARE_CXX_TYPE(
    FLOAT8ARRAYOID,
    AbstractionLayer::MutableArrayHandle<double>,
    dbal::ArrayType,
    dbal::Mutable,
    PointerGetDatum(value.array()),
    reinterpret_cast<ArrayType*>(
        needMutableClone
          ? DatumGetArrayTypePCopy(value)
          : DatumGetArrayTypeP(value))
);

#undef DECLARE_OID_TO_TYPE_MAPPING
#undef DECLARE_CXX_TYPE
#undef DECLARE_OID

#endif // MADLIB_DBCONNECTOR_HPP (workaround for Doxygen)
