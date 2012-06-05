/* ----------------------------------------------------------------------- *//**
 *
 * @file metric.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Find the column in a matrix that is closest to a vector
 */
DECLARE_UDF(linalg, closest_column)


/**
 * @brief Compute the 2-norm
 */
DECLARE_UDF(linalg, norm2)

/**
 * @brief Compute the 1-norm
 */
DECLARE_UDF(linalg, norm1)

/**
 * @brief Compute the Euclidean distance between two dense vectors
 */
DECLARE_UDF(linalg, dist_norm2)

/**
 * @brief Compute the squared Manhattan distance between two dense vectors
 */
DECLARE_UDF(linalg, dist_norm1)

/**
 * @brief Compute the squared Euclidean distance between two dense vectors
 */
DECLARE_UDF(linalg, squared_dist_norm2)

/**
 * @brief Compute the squared Manhattan distance between two dense vectors
 */
DECLARE_UDF(linalg, squared_dist_norm1)


#ifndef MADLIB_MODULES_LINALG_LINALG_HPP
#define MADLIB_MODULES_LINALG_LINALG_HPP

namespace madlib {

namespace modules {

namespace linalg {

template <typename Derived, typename OtherDerived>
std::tuple<dbal::eigen_integration::Index, double>
closestColumnAndDistance(const Eigen::MatrixBase<Derived>& inMatrix,
    const Eigen::MatrixBase<OtherDerived>& inVector, FunctionHandle& inMetric);

} // namespace linalg

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_LINALG_LINALG_HPP)
