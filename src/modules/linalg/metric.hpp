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
 * @brief Find the columns in a matrix that are closest to a vector
 */
DECLARE_UDF(linalg, closest_columns)


/**
 * @brief Compute the 1-norm
 */
DECLARE_UDF(linalg, norm1)

/**
 * @brief Compute the 2-norm
 */
DECLARE_UDF(linalg, norm2)

/**
 * @brief Compute the squared Manhattan distance between two dense vectors
 */
DECLARE_UDF(linalg, dist_norm1)

/**
 * @brief Compute the Euclidean distance between two dense vectors
 */
DECLARE_UDF(linalg, dist_norm2)

/**
 * @brief Compute the squared Euclidean distance between two dense vectors
 */
DECLARE_UDF(linalg, squared_dist_norm2)

/**
 * @brief Compute the angle between two dense vectors
 */
DECLARE_UDF(linalg, dist_angle)

/**
 * @brief Compute the Tanimoto "distance" between two dense vectors
 */
DECLARE_UDF(linalg, dist_tanimoto)


#ifndef MADLIB_MODULES_LINALG_LINALG_HPP
#define MADLIB_MODULES_LINALG_LINALG_HPP

namespace madlib {

namespace modules {

namespace linalg {

// Use Eigen
using namespace dbal::eigen_integration;

template <class DistanceFunction, class RandomAccessIterator>
void
closestColumnsAndDistances(
    const MappedMatrix& inMatrix,
    const MappedColumnVector& inVector,
    DistanceFunction& inMetric,
    RandomAccessIterator ioFirst,
    RandomAccessIterator ioLast);

} // namespace linalg

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_LINALG_LINALG_HPP)
