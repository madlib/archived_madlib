/* ----------------------------------------------------------------------- *//**
 *
 * @file svd.hpp
 *
 * @brief Singular Value Decomposition
 *
 *//* ----------------------------------------------------------------------- */

DECLARE_UDF(linalg, svd_unit_vector)

DECLARE_UDF(linalg, svd_lanczos_sfunc)
DECLARE_UDF(linalg, svd_lanczos_prefunc)

DECLARE_UDF(linalg, svd_lanczos_pvec)
DECLARE_UDF(linalg, svd_lanczos_qvec)

DECLARE_UDF(linalg, svd_gram_schmidt_orthogonalize_sfunc)
DECLARE_UDF(linalg, svd_gram_schmidt_orthogonalize_ffunc)
DECLARE_UDF(linalg, svd_gram_schmidt_orthogonalize_prefunc)

DECLARE_UDF(linalg, svd_decompose_bidiagonal_sfunc)
DECLARE_UDF(linalg, svd_decompose_bidiagonal_ffunc)
DECLARE_UDF(linalg, svd_decompose_bidiagonal_prefunc)

DECLARE_UDF(linalg, svd_block_lanczos_sfunc)
DECLARE_UDF(linalg, svd_sparse_lanczos_sfunc)
DECLARE_UDF(linalg, svd_decompose_bidiag)

DECLARE_UDF(linalg, svd_vec_mult_matrix)
DECLARE_SR_UDF(linalg, svd_vec_trans_mult_matrix)
