/* ----------------------------------------------------------------------- *//**
 *
 * @file matrix_decomp.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Aggregate matrix from dense representation: transition
 */
DECLARE_UDF(linalg, matrix_compose_dense_transition)

/**
 * @brief Aggregate matrix from sparse representation: transition
 */
DECLARE_UDF(linalg, matrix_compose_sparse_transition)

/**
 * @brief Aggregate matrix: merge
 */
DECLARE_UDF(linalg, matrix_compose_merge)

/**
 * @brief Return the inverse of a matrix
 */
DECLARE_UDF(linalg, matrix_inv)
