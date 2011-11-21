#define DECLARE_CXX_TYPE(_oid, _type, _isArray, _convertToPG, _convertToCXX) \
    template <> \
    struct AbstractionLayer::TypeBridge<_type> { \
        enum { oid = _oid }; \
        enum { isArray = _isArray }; \
        static inline Datum toDatum(const _type &value) { \
            return _convertToPG; \
        }; \
        static inline _type toCXXType(Datum value) { \
            return _convertToCXX; \
        }; \
    }

#define DECLARE_OID(_oid, _type) \
    template <> \
    struct AbstractionLayer::TypeForOid<_oid> { \
        typedef _type type; \
    }

#define DECLARE_OID_TO_TYPE_MAPPING(_oid, _type, _isArray, _convertToPG, _convertToCXX) \
    DECLARE_CXX_TYPE(_oid, _type, _isArray, _convertToPG, _convertToCXX); \
    DECLARE_OID(_oid, _type)

DECLARE_OID_TO_TYPE_MAPPING(
    FLOAT8OID,
    double,
    dbal::SimpleType,
    Float8GetDatum(value),
    DatumGetFloat8(value)
);
DECLARE_OID_TO_TYPE_MAPPING(
    FLOAT4OID,
    float,
    dbal::SimpleType,
    Float4GetDatum(value),
    DatumGetFloat4(value)
);
DECLARE_OID_TO_TYPE_MAPPING(
    INT8OID,
    int64_t,
    dbal::SimpleType,
    Int64GetDatum(value),
    DatumGetInt64(value)
);
DECLARE_OID_TO_TYPE_MAPPING(
    INT4OID,
    int32_t,
    dbal::SimpleType,
    Int32GetDatum(value),
    DatumGetInt32(value)
);
DECLARE_OID_TO_TYPE_MAPPING(
    FLOAT8ARRAYOID,
    AbstractionLayer::ArrayHandle<double>,
    dbal::ArrayType,
    PointerGetDatum(value.array()),
    reinterpret_cast< ::ArrayType* >(DatumGetPointer(value))
);
DECLARE_CXX_TYPE(
    FLOAT8ARRAYOID,
    AbstractionLayer::MutableArrayHandle<double>,
    dbal::ArrayType,
    PointerGetDatum(value.array()),
    reinterpret_cast< ::ArrayType* >(DatumGetPointer(value))
);

#undef DECLARE_OID_TO_TYPE_MAPPING
#undef DECLARE_CXX_TYPE
#undef DECLARE_OID
