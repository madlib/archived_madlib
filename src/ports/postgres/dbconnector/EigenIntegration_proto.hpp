/* ----------------------------------------------------------------------- *//**
 *
 * @file EigenIntegration_proto.cpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_EIGEN_INTEGRATION_PROTO_HPP
#define MADLIB_POSTGRES_EIGEN_INTEGRATION_PROTO_HPP

namespace madlib {

namespace dbal {

namespace eigen_integration {

// Specializations for DBMS-specific types

typedef HandleMap<const ColumnVector, ArrayHandle<double> > NativeColumnVector;
typedef HandleMap<ColumnVector, MutableArrayHandle<double> >
    MutableNativeColumnVector;
typedef HandleMap<const Matrix, ArrayHandle<double> > NativeMatrix;
typedef HandleMap<Matrix, MutableArrayHandle<double> > MutableNativeMatrix;
typedef HandleMap<const IntegerVector, ArrayHandle<int> > NativeIntegerVector;
typedef HandleMap<IntegerVector, MutableArrayHandle<int> >
    MutableNativeIntegerVector;

typedef HandleMap<const ColumnVector, TransparentHandle<double> > MappedColumnVector;
typedef HandleMap<ColumnVector, TransparentHandle<double, Mutable> >
    MutableMappedColumnVector;
typedef HandleMap<const Matrix, TransparentHandle<double> > MappedMatrix;
typedef HandleMap<Matrix, TransparentHandle<double, Mutable> > MutableMappedMatrix;
typedef HandleMap<const IntegerVector, TransparentHandle<int> > MappedIntegerVector;
typedef HandleMap<IntegerVector, TransparentHandle<int, Mutable> >
    MutableMappedIntegerVector;
typedef HandleMap<const VectorXcd, TransparentHandle<std::complex<double> > >
    MappedVectorXcd;
typedef HandleMap<VectorXcd, TransparentHandle<std::complex<double> > >
    MutableMappedVectorXcd;

} // namespace eigen_integration

// DynamicStructType
template <bool IsMutable>
struct DynamicStructType<eigen_integration::MappedColumnVector, IsMutable> {
    typedef typename
        DynamicStructType<eigen_integration::ColumnVector, IsMutable>::type
        type;
};

} // namespace dbal


namespace dbconnector {

namespace postgres {

Eigen::SparseVector<double> LegacySparseVectorToSparseColumnVector(
    SvecType* inVec);

SvecType* SparseColumnVectorToLegacySparseVector(
    const Eigen::SparseVector<double> &inVec);

template <typename Derived>
ArrayType* VectorToNativeArray(const Eigen::MatrixBase<Derived>& inVector);

template <typename Derived>
ArrayType* MatrixToNativeArray(const Eigen::MatrixBase<Derived>& inMatrix);

template <class MatrixType>
MatrixType
NativeArrayToMappedMatrix(Datum inDatum, bool inNeedMutableClone);

template <class VectorType>
VectorType
NativeArrayToMappedVector(Datum inDatum, bool inNeedMutableClone);

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_EIGEN_INTEGRATION_PROTO_HPP)
