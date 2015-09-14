/* ----------------------------------------------------------------------- *//**
 *
 * @file TypeTraits_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_TYPETRAITS_IMPL_HPP
#define MADLIB_POSTGRES_TYPETRAITS_IMPL_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

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
    static const char* typeName() { \
        return NULL; \
    }

/*
 * Mutable means in this context that the value of a variable can be changed and
 * that it would change the value for the backend
 */
#define WITH_MUTABILITY(_isMutable) \
    enum { isMutable = _isMutable }
#define WITHOUT_SYSINFO \
    static SystemInformation* toSysInfo(const value_type&) { \
        return NULL; \
    }
#define WITH_DEFAULT_EXTENDED_TRAITS \
    WITHOUT_TYPE_NAME \
    WITHOUT_SYSINFO
#define WITH_SYS_INFO_CONVERSION(_convertToSysInfo) \
    static SystemInformation* toSysInfo(const value_type& value) { \
        (void) value; \
        return _convertToSysInfo; \
    }
#define WITH_TO_PG_CONVERSION(_convertToPG) \
    static Datum toDatum(const value_type& value) { \
        return _convertToPG; \
    }
#define WITH_TO_CXX_CONVERSION(_convertToCXX) \
    static value_type toCXXType(Datum value, bool needMutableClone, \
        SystemInformation* sysInfo) { \
        \
        (void) value; \
        (void) needMutableClone; \
        (void) sysInfo; \
        return _convertToCXX; \
    }
#define WITH_BIND_TO_STREAM(_bindToStream) \
    template <class StreamBuf> \
    static void bindToStream(dbal::ByteStream<StreamBuf>& stream, \
        reference_type& ref) { \
        \
        _bindToStream; \
    }


template <typename T>
struct TypeTraitsBase {
    enum { oid = InvalidOid };

    enum { alignment = MAXIMUM_ALIGNOF };
    enum { isMutable = dbal::Immutable };
    enum { typeClass = dbal::SimpleType };

    typedef T value_type;

    static const char* typeName() {
        return NULL;
    }

    static SystemInformation* toSysInfo(const value_type&) {
        return NULL;
    }
};

template <>
struct TypeTraits<double> : public TypeTraitsBase<double> {
    enum { oid = FLOAT8OID };
    enum { alignment = ALIGNOF_DOUBLE };
    WITH_TO_PG_CONVERSION( Float8GetDatum(value) );
    WITH_TO_CXX_CONVERSION( DatumGetFloat8(value) );
};

template <>
struct TypeTraits<float>{
    typedef float value_type;

    WITH_OID( FLOAT4OID );
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( Float4GetDatum(value) );
    WITH_TO_CXX_CONVERSION( DatumGetFloat4(value) );
};

template <>
struct TypeTraits<int64_t> : public TypeTraitsBase<int64_t> {
    enum { oid = INT8OID };
    enum { alignment = ALIGNOF_LONG };
    WITH_TO_PG_CONVERSION( Int64GetDatum(value) );
    WITH_TO_CXX_CONVERSION( DatumGetInt64(value) );
};

template <>
struct TypeTraits<uint64_t> : public TypeTraitsBase<uint64_t> {
    enum { oid = INT8OID };
    enum { alignment = ALIGNOF_LONG };
    WITH_TO_PG_CONVERSION(
        Int64GetDatum((convertTo<uint64_t, int64_t>(value)))
    );
    WITH_TO_CXX_CONVERSION(
        (convertTo<int64_t, uint64_t>(DatumGetInt64(value)))
    );
};

template <>
struct TypeTraits<int32_t> : public TypeTraitsBase<int32_t> {
    enum { oid = INT4OID };
    enum { alignment = ALIGNOF_INT };
    WITH_TO_PG_CONVERSION( Int32GetDatum(value) );
    WITH_TO_CXX_CONVERSION( DatumGetInt32(value) );
};

template <>
struct TypeTraits<uint32_t> : public TypeTraitsBase<uint32_t> {
    enum { oid = INT4OID };
    enum { alignment = ALIGNOF_INT };
    WITH_TO_PG_CONVERSION(
        Int32GetDatum((convertTo<uint32_t, int32_t>(value)))
    );
    WITH_TO_CXX_CONVERSION(
        (convertTo<int32_t, uint32_t>(DatumGetInt32(value)))
    );
};

template <>
struct TypeTraits<int16_t> : public TypeTraitsBase<int16_t> {
    enum { oid = INT2OID };
    enum { alignment = ALIGNOF_SHORT };
    WITH_TO_PG_CONVERSION( Int16GetDatum(value) );
    WITH_TO_CXX_CONVERSION( DatumGetInt16(value) );
};

template <>
struct TypeTraits<uint16_t> : public TypeTraitsBase<uint16_t> {
    enum { oid = INT2OID };
    enum { alignment = ALIGNOF_SHORT };
    WITH_TO_PG_CONVERSION(
        Int16GetDatum((convertTo<uint16_t, int16_t>(value)))
    );
    WITH_TO_CXX_CONVERSION(
        (convertTo<int16_t, uint16_t>(DatumGetInt16(value)))
    );
};

template <>
struct TypeTraits<bool> : public TypeTraitsBase<bool> {
    enum { oid = BOOLOID };
    enum { alignment = 1 };

    WITH_TO_PG_CONVERSION( BoolGetDatum(value) );
    WITH_TO_CXX_CONVERSION( DatumGetBool(value) );
};

template <>
struct TypeTraits<char*> {
    typedef char* value_type;

    WITH_OID( TEXTOID );
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION(PointerGetDatum(cstring_to_text(value)));
    WITH_TO_CXX_CONVERSION(text_to_cstring(DatumGetTextPP(value)));
};

template <>
struct TypeTraits<std::string> {
    typedef std::string value_type;

    WITH_OID( TEXTOID );
    WITH_TYPE_CLASS( dbal::SimpleType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION(PointerGetDatum(
            cstring_to_text_with_len(value.data(),
                                     static_cast<int>(value.size()))));
    WITH_TO_CXX_CONVERSION(std::string(VARDATA_ANY(value),
                                       VARSIZE_ANY(value) - VARHDRSZ));
};

template <>
struct TypeTraits<ByteString> : public TypeTraitsBase<ByteString> {
    enum { alignment = MAXIMUM_ALIGNOF };
    WITH_TYPE_NAME("bytea8");
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.byteString()) );
    WITH_TO_CXX_CONVERSION( madlib_DatumGetByteaP(value) );
};

template <>
struct TypeTraits<MutableByteString> : public TypeTraitsBase<MutableByteString> {
    enum { alignment = MAXIMUM_ALIGNOF };
    WITH_MUTABILITY( dbal::Mutable );
    WITH_TYPE_NAME("bytea8");
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.byteString()) );
    WITH_TO_CXX_CONVERSION(
            needMutableClone
          ? madlib_DatumGetByteaPCopy(value)
          : madlib_DatumGetByteaP(value)
    );
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
struct TypeTraits<ArrayHandle<int32_t> >
  : public TypeTraitsBase<ArrayHandle<int32_t> > {
    enum { oid = INT4ARRAYOID };
    enum { isMutable = dbal::Immutable };
    enum { typeClass = dbal::ArrayType };
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.array()) );
    WITH_TO_CXX_CONVERSION( madlib_DatumGetArrayTypeP(value) );
};

template <>
struct TypeTraits<MutableArrayHandle<int32_t> >
  : public TypeTraitsBase<MutableArrayHandle<int32_t> > {
    enum { oid = INT4ARRAYOID };
    enum { isMutable = dbal::Mutable };
    enum { typeClass = dbal::ArrayType };
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.array()) );
    WITH_TO_CXX_CONVERSION(
        needMutableClone
          ? madlib_DatumGetArrayTypePCopy(value)
          : madlib_DatumGetArrayTypeP(value)
    );
};

template <>
struct TypeTraits<ArrayHandle<int64_t> >
  : public TypeTraitsBase<ArrayHandle<int64_t> > {
    enum { oid = INT8ARRAYOID };
    enum { isMutable = dbal::Immutable };
    enum { typeClass = dbal::ArrayType };
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.array()) );
    WITH_TO_CXX_CONVERSION( madlib_DatumGetArrayTypeP(value) );
};

template <>
struct TypeTraits<MutableArrayHandle<int64_t> >
  : public TypeTraitsBase<MutableArrayHandle<int64_t> > {
    enum { oid = INT8ARRAYOID };
    enum { isMutable = dbal::Mutable };
    enum { typeClass = dbal::ArrayType };
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.array()) );
    WITH_TO_CXX_CONVERSION(
        needMutableClone
          ? madlib_DatumGetArrayTypePCopy(value)
          : madlib_DatumGetArrayTypeP(value)
    );
};

template <>
struct TypeTraits<ArrayHandle<double> > {
    typedef ArrayHandle<double> value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.array()) );
    WITH_TO_CXX_CONVERSION( madlib_DatumGetArrayTypeP(value) );
};

template <>
struct TypeTraits<ArrayHandle<text*> > {
    typedef ArrayHandle<text*> value_type;

    WITH_OID(TEXTARRAYOID);
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

// NativeColumnVector
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
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.memoryHandle().array()) );
    WITH_TO_CXX_CONVERSION(
        ArrayHandle<double>(reinterpret_cast<ArrayType*>(
            madlib_DatumGetArrayTypeP(value)))
    );
};

// MutableNativeColumnVector
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

// NativeIntegerVector
template <>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::IntegerVector,
        ArrayHandle<int> > > {

    typedef dbal::eigen_integration::HandleMap<
        const dbal::eigen_integration::IntegerVector,
        ArrayHandle<int> > value_type;

    WITH_OID( INT4ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.memoryHandle().array()) );
    WITH_TO_CXX_CONVERSION(
        ArrayHandle<int>(reinterpret_cast<ArrayType*>(
            madlib_DatumGetArrayTypeP(value)))
    );
};

// MutableNativeIntegerVector
template <>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        dbal::eigen_integration::IntegerVector,
        MutableArrayHandle<int> > > {

    typedef dbal::eigen_integration::HandleMap<
        dbal::eigen_integration::IntegerVector,
        MutableArrayHandle<int> > value_type;

    WITH_OID( INT4ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Mutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(value.memoryHandle().array()) );
    WITH_TO_CXX_CONVERSION(
        MutableArrayHandle<int>(reinterpret_cast<ArrayType*>(
            needMutableClone
                ? madlib_DatumGetArrayTypePCopy(value)
                : madlib_DatumGetArrayTypeP(value)
        ))
    );
};

// NativeMatrix
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

// MutableNativeMatrix
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

// MappedColumnVector and MutableMappedColumnVector
// FIXME: This looks gross. We want to express this without hurting our eyes.
template <bool IsMutable>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            dbal::eigen_integration::ColumnVector,
            const dbal::eigen_integration::ColumnVector>::type,
        TransparentHandle<double, IsMutable> > >
  : public TypeTraitsBase<dbal::eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            dbal::eigen_integration::ColumnVector,
            const dbal::eigen_integration::ColumnVector>::type,
        TransparentHandle<double, IsMutable> > > {

    typedef TypeTraitsBase<dbal::eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            dbal::eigen_integration::ColumnVector,
            const dbal::eigen_integration::ColumnVector>::type,
        TransparentHandle<double, IsMutable> > > Base;
    typedef typename Base::value_type value_type;

    enum { oid = FLOAT8ARRAYOID };
    enum { alignment = MAXIMUM_ALIGNOF };
    enum { isMutable = IsMutable };
    enum { typeClass = dbal::ArrayType };

    WITH_TO_PG_CONVERSION( PointerGetDatum(VectorToNativeArray(value)) )
    WITH_TO_CXX_CONVERSION((
        NativeArrayToMappedVector<value_type>(value, needMutableClone)
    ));
};

// MappedIntegerVector and MutableMappedIntegerVector
template <bool IsMutable>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            dbal::eigen_integration::IntegerVector,
            const dbal::eigen_integration::IntegerVector>::type,
        TransparentHandle<int, IsMutable> > >
  : public TypeTraitsBase<dbal::eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            dbal::eigen_integration::IntegerVector,
            const dbal::eigen_integration::IntegerVector>::type,
        TransparentHandle<int, IsMutable> > > {

    typedef TypeTraitsBase<dbal::eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            dbal::eigen_integration::IntegerVector,
            const dbal::eigen_integration::IntegerVector>::type,
        TransparentHandle<int, IsMutable> > > Base;
    typedef typename Base::value_type value_type;

    enum { oid = INT4ARRAYOID };
    enum { alignment = MAXIMUM_ALIGNOF };
    enum { isMutable = IsMutable };
    enum { typeClass = dbal::ArrayType };

    WITH_TO_PG_CONVERSION( PointerGetDatum(VectorToNativeArray(value)) )
    WITH_TO_CXX_CONVERSION((
        NativeArrayToMappedVector<value_type>(value, needMutableClone)
    ));
};

// MappedMatrix and MutableMappedMatrix
template <bool IsMutable>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            dbal::eigen_integration::Matrix,
            const dbal::eigen_integration::Matrix>::type,
        TransparentHandle<double, IsMutable> > >
  : public TypeTraitsBase<dbal::eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            dbal::eigen_integration::Matrix,
            const dbal::eigen_integration::Matrix>::type,
        TransparentHandle<double, IsMutable> > > {

    typedef TypeTraitsBase<dbal::eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            dbal::eigen_integration::Matrix,
            const dbal::eigen_integration::Matrix>::type,
        TransparentHandle<double, IsMutable> > > Base;
    typedef typename Base::value_type value_type;

    enum { oid = FLOAT8ARRAYOID };
    enum { alignment = MAXIMUM_ALIGNOF };
    enum { isMutable = IsMutable };
    enum { typeClass = dbal::ArrayType };

    WITH_TO_PG_CONVERSION( PointerGetDatum(MatrixToNativeArray(value)) )
    WITH_TO_CXX_CONVERSION((
        NativeArrayToMappedMatrix<value_type>(value, needMutableClone)
    ))
};

// MappedVectorXcd and MutableMappedVectorXcd
template <bool IsMutable>
struct TypeTraits<
    dbal::eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            dbal::eigen_integration::VectorXcd,
            const dbal::eigen_integration::VectorXcd>::type,
        TransparentHandle<std::complex<double>, IsMutable> > >
  : public TypeTraitsBase<dbal::eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            dbal::eigen_integration::VectorXcd,
            const dbal::eigen_integration::VectorXcd>::type,
        TransparentHandle<std::complex<double>, IsMutable> > > {

    typedef TypeTraitsBase<dbal::eigen_integration::HandleMap<
        typename boost::mpl::if_c<IsMutable,
            dbal::eigen_integration::VectorXcd,
            const dbal::eigen_integration::VectorXcd>::type,
        TransparentHandle<std::complex<double>, IsMutable> > > Base;
    typedef typename Base::value_type value_type;

    enum { oid = FLOAT8ARRAYOID };
    enum { alignment = MAXIMUM_ALIGNOF };
    enum { isMutable = IsMutable };
    enum { typeClass = dbal::ArrayType };

    WITH_TO_PG_CONVERSION( PointerGetDatum(VectorXcdToNativeArray(value)) )
    WITH_TO_CXX_CONVERSION((
        NativeArrayToMappedVectorXcd<value_type>(value, needMutableClone)
    ))
};

// ------------------------------------------------------------------------
// locally allocated structures require copying
// ------------------------------------------------------------------------
template <>
struct TypeTraits<dbal::eigen_integration::ColumnVector>
  : public TypeTraitsBase<dbal::eigen_integration::ColumnVector> {
    typedef dbal::eigen_integration::ColumnVector value_type;

    enum { oid = FLOAT8ARRAYOID };
    enum { isMutable = dbal::Immutable };
    enum { typeClass = dbal::ArrayType };
    WITH_TO_PG_CONVERSION( PointerGetDatum(VectorToNativeArray(value)) );
    // No need to support retrieving this type from the backend. Use
    // MappedColumnVector instead.
};

template <>
struct TypeTraits<dbal::eigen_integration::IntegerVector>
  : public TypeTraitsBase<dbal::eigen_integration::IntegerVector> {
    typedef dbal::eigen_integration::IntegerVector value_type;

    enum { oid = INT4ARRAYOID };
    enum { isMutable = dbal::Immutable };
    enum { typeClass = dbal::ArrayType };
    WITH_TO_PG_CONVERSION( PointerGetDatum(VectorToNativeArray(value)) );
    // No need to support retrieving this type from the backend. Use
    // MappedIntegerVector instead.
};

template <>
struct TypeTraits<dbal::eigen_integration::Matrix>
  : public TypeTraitsBase<dbal::eigen_integration::Matrix> {
    typedef dbal::eigen_integration::Matrix value_type;

    enum { oid = FLOAT8ARRAYOID };
    enum { isMutable = dbal::Immutable };
    enum { typeClass = dbal::ArrayType };
    WITH_TO_PG_CONVERSION( PointerGetDatum(MatrixToNativeArray(value)) );
    // No need to support retrieving this type from the backend. Use
    // MappedMatrix instead.
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
    // MappedColumnVector or MappedMatrix instead.
};

template<class XprType, int BlockRows, int BlockCols, bool InnerPanel>
struct TypeTraits<
    Eigen::Block<XprType, BlockRows, BlockCols, InnerPanel> > {

    typedef Eigen::Block<XprType, BlockRows, BlockCols, InnerPanel> value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(MatrixToNativeArray(value)) );
    // No need to support retrieving this type from the backend. Use
    // MappedMatrix instead.
};

template<class XprType, int BlockRows, bool InnerPanel>
struct TypeTraits<
    Eigen::Block<XprType, BlockRows, /* BlockCols */ 1, InnerPanel> > {

    typedef Eigen::Block<XprType, BlockRows, 1, InnerPanel> value_type;

    WITH_OID( FLOAT8ARRAYOID );
    WITH_TYPE_CLASS( dbal::ArrayType );
    WITH_MUTABILITY( dbal::Immutable );
    WITH_DEFAULT_EXTENDED_TRAITS;
    WITH_TO_PG_CONVERSION( PointerGetDatum(VectorToNativeArray(value)) );
    // No need to support retrieving this type from the backend. Use
    // MappedColumnVector instead.
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

// Special cases

template <>
struct TypeTraits<dbal::ByteStreamMaximumAlignmentType>
  : public TypeTraitsBase<dbal::ByteStreamMaximumAlignmentType> {

    enum { alignment = MAXIMUM_ALIGNOF };
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

#endif // defined(MADLIB_POSTGRES_TYPETRAITS_IMPL_HPP)
