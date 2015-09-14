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

/**
 * @brief Return the eigen values of a matrix
 */
DECLARE_UDF(linalg, matrix_eigen)

/**
 * @brief Return the standard cholesky decomposition of a matrix
 */
DECLARE_UDF(linalg, matrix_cholesky)

/**
 * @brief Return the standard qr decomposition of a matrix
 */
DECLARE_UDF(linalg, matrix_qr)

/**
 * @brief Return the rank of a matrix
 */
DECLARE_UDF(linalg, matrix_rank)

/**
 * @brief Return the LU decomposition of a matrix
 */
DECLARE_UDF(linalg, matrix_lu)

/**
 * @brief Return the nuclear norm of a matrix
 */
DECLARE_UDF(linalg, matrix_nuclear_norm)

/**
 * @brief Return the generalized inverse of a matrix
 */
DECLARE_UDF(linalg, matrix_pinv)
