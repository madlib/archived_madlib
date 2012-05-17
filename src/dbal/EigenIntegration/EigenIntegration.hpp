/* ----------------------------------------------------------------------- *//**
 *
 * @file EigenIntegration.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_EIGEN_INTEGRATION_HPP
#define MADLIB_DBAL_EIGEN_INTEGRATION_HPP

// Plugin with methods to add to Eigen's MatrixBase
#define EIGEN_MATRIXBASE_PLUGIN <dbal/EigenIntegration/EigenPlugin.hpp>
#include <Eigen/Dense>

#define EIGEN_YES_I_KNOW_SPARSE_MODULE_IS_NOT_STABLE_YET
#include <Eigen/Sparse>

namespace madlib {

namespace dbal {

namespace eigen_integration {

typedef Eigen::VectorXd ColumnVector;
typedef Eigen::RowVectorXd RowVector;
typedef Eigen::MatrixXd Matrix;
typedef EIGEN_DEFAULT_DENSE_INDEX_TYPE Index;

typedef Eigen::SparseVector<double> SparseColumnVector;

enum ViewMode {
    Lower = Eigen::Lower,
    Upper = Eigen::Upper
};

enum DecompositionOptions {
    ComputeEigenvectors = Eigen::ComputeEigenvectors,
    EigenvaluesOnly = Eigen::EigenvaluesOnly
};

enum SPDDecompositionExtras {
    ComputePseudoInverse = 0x01
};

// In the following we make several definitions that allow certain object
// methods to be called as a plain functions. This makes using Eigen more
// similar to Armadillo.

template <typename Derived>
inline
typename Eigen::MatrixBase<Derived>::ConstTransposeReturnType
static trans(const Eigen::MatrixBase<Derived>& mat) {
    return mat.transpose();
}

template <typename Derived, typename OtherDerived>
inline
typename Eigen::internal::scalar_product_traits<
    typename Eigen::internal::traits<Derived>::Scalar,
    typename Eigen::internal::traits<OtherDerived>::Scalar
>::ReturnType
static dot(
    const Eigen::MatrixBase<Derived>& mat,
    const Eigen::MatrixBase<OtherDerived>& other) {
    return mat.dot(other);
}

template <typename Derived>
inline
typename Eigen::MatrixBase<Derived>::CoeffReturnType
static as_scalar(const Eigen::MatrixBase<Derived>& mat) {
    return mat.value();
}

template <ViewMode Mode, typename Derived>
inline
typename Eigen::MatrixBase<Derived>::template TriangularViewReturnType<Mode>::Type
static triangularView(Eigen::MatrixBase<Derived>& mat) {
    return mat.triangularView<Mode>();
}

template <typename Derived>
bool
static isfinite(const Eigen::MatrixBase<Derived>& mat) {
    return mat.is_finite();
}

} // namespace eigen_integration

} // namespace dbal

} // namespace madlib

#include "HandleMap_proto.hpp"
#include "SymmetricPositiveDefiniteEigenDecomposition_proto.hpp"

#include "HandleMap_impl.hpp"
#include "SymmetricPositiveDefiniteEigenDecomposition_impl.hpp"

#endif // defined(MADLIB_EIGEN_INTEGRATION_HPP)
