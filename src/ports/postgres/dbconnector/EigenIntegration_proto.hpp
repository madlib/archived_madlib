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

template <>
HandleMap<const Matrix, ArrayHandle<double> >::HandleMap(
    const ArrayHandle<double>& inHandle);

template <>
HandleMap<Matrix, MutableArrayHandle<double> >::HandleMap(
    const MutableArrayHandle<double>& inHandle);

typedef HandleMap<const ColumnVector, ArrayHandle<double> > MappedColumnVector;
typedef HandleMap<ColumnVector, MutableArrayHandle<double> >
    MutableMappedColumnVector;
typedef HandleMap<const Matrix, ArrayHandle<double> > MappedMatrix;
typedef HandleMap<Matrix, MutableArrayHandle<double> > MutableMappedMatrix;

} // namespace dbal

} // namespace eigen_integration


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

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_EIGEN_INTEGRATION_PROTO_HPP)
