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

// The second template parameter can be used for SFINAE.
template <typename T, class = void>
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

#define WITH_OID(_oid) enum { oid = _oid }

/*
 * The type OID (_oid) can be set to InvalidOid if it should not be used for
 * type verification. This is the case for types that are not built-in and for
 * which no fixed OID is known at compile time.
 */
#define WITHOUT_OID enum { oid = InvalidOid }

#define WITH_TYPE_CLASS(_typeClass) enum { typeClass = _typeClass }

#define WITH_TYPE_NAME(_type_name) \
    static inline const char* typeName() { \
        return _type_name; \
    }

/* The type name (_type_name) can be NULL if it should not be used for type
 * verification. This is the case for built-in types, for which only the type
 * OID should be verified, but not the type name.
 */
#define WITHOUT_TYPE_NAME \
    static inline const char* typeName() { \
        return NULL; \
    }

/*
 * Mutable means in this context that the value of a variable can be changed and
 * that it would change the value for the backend
 */
#define WITH_MUTABILITY(_isMutable) \
    enum { isMutable = _isMutable }
#define WITHOUT_SYSINFO \
    static inline SystemInformation* toSysInfo(const value_type&) { \
        return NULL; \
    }
#define WITH_DEFAULT_EXTENDED_TRAITS \
    WITHOUT_TYPE_NAME \
    WITHOUT_SYSINFO
#define WITH_SYS_INFO_CONVERSION(_convertToSysInfo) \
    static inline SystemInformation* toSysInfo(const value_type& value) { \
        (void) value; \
        return _convertToSysInfo; \
    }
#define WITH_TO_PG_CONVERSION(_convertToPG) \
    static inline Datum toDatum(const value_type& value) { \
        return _convertToPG; \
    }
#define WITH_TO_CXX_CONVERSION(_convertToCXX) \
    static inline value_type toCXXType(Datum value, bool needMutableClone, \
        SystemInformation* sysInfo) { \
        \
        (void) value; \
        (void) needMutableClone; \
        (void) sysInfo; \
        return _convertToCXX; \
    }

template <>
struct TypeTraits<double> {
    typedef double value_type;

    WITH_OID( FLOAT8OID );
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( Float8GetDatum(value) );
    WITH_TO_CXX_CONVERSION( DatumGetFloat8(value) );
};

template <>
struct TypeTraits<float> {
    typedef float value_type;

    WITH_OID( FLOAT4OID );
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( Float4GetDatum(value) );
    WITH_TO_CXX_CONVERSION( DatumGetFloat4(value) );
};

template <>
struct TypeTraits<int64_t> {
    typedef int64_t value_type;

    WITH_OID( INT8OID );
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( Int64GetDatum(value) );
    WITH_TO_CXX_CONVERSION( DatumGetInt64(value) );
};

template <>
struct TypeTraits<uint64_t> {
    typedef uint64_t value_type;

    WITH_OID( INT8OID );
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION(
        Int64GetDatum((convertTo<uint64_t, int64_t>(value)))
    );
    WITH_TO_CXX_CONVERSION(
        (convertTo<int64_t, uint64_t>(DatumGetInt64(value)))
    );
};

template <>
struct TypeTraits<int32_t> {
    typedef int32_t value_type;

    WITH_OID( INT4OID );
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( Int32GetDatum(value) );
    WITH_TO_CXX_CONVERSION( DatumGetInt32(value) );
};

template <>
struct TypeTraits<uint32_t> {
    typedef uint32_t value_type;

    WITH_OID( INT4OID );
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION(
        Int32GetDatum((convertTo<uint32_t, int32_t>(value)))
    );
    WITH_TO_CXX_CONVERSION(
        (convertTo<int32_t, uint32_t>(DatumGetInt32(value)))
    );
};

template <>
struct TypeTraits<int16_t> {
    typedef int16_t value_type;

    WITH_OID( INT2OID );
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( Int16GetDatum(value) );
    WITH_TO_CXX_CONVERSION( DatumGetInt16(value) );
};

template <>
struct TypeTraits<bool> {
    typedef bool value_type;

    WITH_OID( BOOLOID );
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( BoolGetDatum(value) );
    WITH_TO_CXX_CONVERSION( DatumGetBool(value) );
};

template <>
struct TypeTraits<FunctionHandle> {
    typedef FunctionHandle value_type;

    WITH_OID( REGPROCOID );
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITHOUT_TYPE_NAME;
    WITH_SYS_INFO_CONVERSION( value.getSysInfo() );
    WITH_TO_PG_CONVERSION( ObjectIdGetDatum(value.funcID()) );
    WITH_TO_CXX_CONVERSION( FunctionHandle(sysInfo, DatumGetObjectId(value)) );
};

template <>
struct TypeTraits<ArrayHandle<double> > {
    typedef ArrayHandle<double> value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.array()) );
    WITH_TO_CXX_CONVERSION(
        reinterpret_cast<ArrayType*>(madlib_DatumGetArrayTypeP(value))
    );
};

// Note: See the comment for PG_FREE_IF_COPY in fmgr.h. Essentially, when
// writing UDFs, we do not have to worry about deallocating copies of immutable
// arrays. They will simply be garbage collected.
template <>
struct TypeTraits<MutableArrayHandle<double> > {
    typedef MutableArrayHandle<double> value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Mutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.array()) );
    WITH_TO_CXX_CONVERSION(
        needMutableClone
          ? madlib_DatumGetArrayTypePCopy(value)
          : madlib_DatumGetArrayTypeP(value)
    );
};

template <>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::ColumnVector,
        ArrayHandle<double> > > {

    typedef dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::ColumnVector,
        ArrayHandle<double> > value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Mutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.memoryHandle().array()) );
    WITH_TO_CXX_CONVERSION(
        ArrayHandle<double>(reinterpret_cast<ArrayType*>(
            madlib_DatumGetArrayTypeP(value)))
    );
};

template <>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        dbal::eigen_integration::ColumnVector,
        MutableArrayHandle<double> > > {

    typedef dbal::eigen_integration::HandleMap<
        dbal::eigen_integration::ColumnVector,
        MutableArrayHandle<double> > value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Mutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.memoryHandle().array()) );
    WITH_TO_CXX_CONVERSION(
        MutableArrayHandle<double>(reinterpret_cast<ArrayType*>(
            needMutableClone
                ? madlib_DatumGetArrayTypePCopy(value)
                : madlib_DatumGetArrayTypeP(value)
        ))
    );
};

template <>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::ColumnVector,
        TransparentHandle<double> > > {

    typedef dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::ColumnVector,
        TransparentHandle<double> > value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(VectorToNativeArray(value)) );
    // No need to support retrieving this type from the backend. Use
    // HandleMap<ColumnVector, ArrayHandle<double> > instead.
};

template <>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        dbal::eigen_integration::ColumnVector,
        MutableTransparentHandle<double> > > {

    typedef dbal::eigen_integration::HandleMap<
        dbal::eigen_integration::ColumnVector,
        MutableTransparentHandle<double> > value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Mutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(VectorToNativeArray(value)) );
    // No need to support retrieving this type from the backend. Use
    // HandleMap<ColumnVector, ArrayHandle<double> > instead.
};

template <>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::Matrix,
        ArrayHandle<double> > > {

    typedef dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::Matrix,
        ArrayHandle<double> > value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.memoryHandle().array()) );
    WITH_TO_CXX_CONVERSION(
        ArrayHandle<double>(
            reinterpret_cast<ArrayType*>(madlib_DatumGetArrayTypeP(value))
        )
    );
};

template <>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        dbal::eigen_integration::Matrix,
        MutableArrayHandle<double> > > {

    typedef dbal::eigen_integration::HandleMap<
        dbal::eigen_integration::Matrix,
        MutableArrayHandle<double> > value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Mutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.memoryHandle().array()) );
    WITH_TO_CXX_CONVERSION(
        MutableArrayHandle<double>(
            reinterpret_cast<ArrayType*>(needMutableClone
                ? madlib_DatumGetArrayTypePCopy(value)
                : madlib_DatumGetArrayTypeP(value)
        ))
    );
};

template <>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::Matrix,
        TransparentHandle<double> > > {

    typedef dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::Matrix,
        TransparentHandle<double> > value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(MatrixToNativeArray(value)) );
    // No need to support retrieving this type from the backend. Use
    // HandleMap<ColumnVector, ArrayHandle<double> > instead.
};

template <>
struct TypeTraits<dbal::eigen_integration::Matrix> {
    typedef dbal::eigen_integration::Matrix value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(MatrixToNativeArray(value)) );
    // No need to support retrieving this type from the backend. Use
    // HandleMap<ColumnVector, ArrayHandle<double> > instead.
};

template<class Derived>
struct TypeTraits<Eigen::MatrixBase<Derived> > {
    typedef Eigen::MatrixBase<Derived> value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION(
        PointerGetDatum(
            Derived::RowsAtCompileTime == 1 || Derived::ColsAtCompileTime == 1
                ? VectorToNativeArray(value)
                : MatrixToNativeArray(value)
        )
    );
    // No need to support retrieving this type from the backend. Use
    // HandleMap<ColumnVector, ArrayHandle<double> > instead.
};

template<class XprType, int BlockRows, bool InnerPanel, bool HasDirectAccess>
struct TypeTraits<
    Eigen::Block<XprType, BlockRows, /* BlockCols */ 1, InnerPanel,
        HasDirectAccess> > {

    typedef Eigen::Block<XprType, BlockRows, 1, InnerPanel,
        HasDirectAccess> value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(VectorToNativeArray(value)) );
    // No need to support retrieving this type from the backend. Use
    // HandleMap<ColumnVector, ArrayHandle<double> > instead.
};

template <>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        dbal::eigen_integration::Matrix,
        MutableTransparentHandle<double> > > {

    typedef dbal::eigen_integration::HandleMap<
        dbal::eigen_integration::Matrix,
        MutableTransparentHandle<double> > value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Mutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(MatrixToNativeArray(value)) );
    // No need to support retrieving this type from the backend. Use
    // HandleMap<ColumnVector, ArrayHandle<double> > instead.
};

template <>
struct TypeTraits<dbal::eigen_integration::SparseColumnVector> {
    typedef dbal::eigen_integration::SparseColumnVector value_type;

    WITHOUT_OID;
    WITH_TYPE_NAME("svec");
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITHOUT_SYSINFO;
    WITH_TO_PG_CONVERSION(
        PointerGetDatum(SparseColumnVectorToLegacySparseVector(value))
    );
    WITH_TO_CXX_CONVERSION(
        LegacySparseVectorToSparseColumnVector(reinterpret_cast<SvecType*>(value))
    );
};

#undef WITH_OID
#undef WITHOUT_OID
#undef WITH_TYPE_CLASS
#undef WITH_TYPE_NAME
#undef WITHOUT_TYPE_NAME
#undef WITH_MUTABILITY
#undef WITHOUT_SYSINFO
#undef WITH_DEFAULT_EXTENDED_TRAITS
#undef WITH_SYS_INFO_CONVERSION
#undef WITH_TO_PG_CONVERSION
#undef WITH_TO_CXX_CONVERSION

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_TYPETRAITS_HPP)
