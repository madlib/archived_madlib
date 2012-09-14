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

/**
 * @brief Compute the squared angle between two dense vectors
 */
DECLARE_UDF(linalg, squared_angle)

/**
 * @brief Compute the squared Tanimoto "distance" between two dense vectors
 */
DECLARE_UDF(linalg, squared_tanimoto)


#ifndef MADLIB_MODULES_LINALG_LINALG_HPP
#define MADLIB_MODULES_LINALG_LINALG_HPP

namespace madlib {

namespace modules {

namespace linalg {

// Use Eigen
using namespace dbal::eigen_integration;

template <class DistanceFunction>
std::tuple<dbal::eigen_integration::Index, double>
closestColumnAndDistance(const MappedMatrix& inMatrix,
    const MappedColumnVector& inVector, DistanceFunction& inMetric);

} // namespace linalg

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_LINALG_LINALG_HPP)
