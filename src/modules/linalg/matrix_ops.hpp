/* ----------------------------------------------------------------------- *//**
 *
 * @file matrix_ops.hpp
 *
 *//* ----------------------------------------------------------------------- */

DECLARE_UDF(linalg, matrix_mem_mult)
// DECLARE_UDF(linalg, matrix_vec_mem_mult)
DECLARE_UDF(linalg, matrix_mem_trans)

DECLARE_UDF(linalg, matrix_densify_sfunc)
DECLARE_UDF(linalg, matrix_blockize_sfunc)
DECLARE_UDF(linalg, matrix_unblockize_sfunc)
DECLARE_UDF(linalg, matrix_mem_sum_sfunc)

DECLARE_UDF(linalg, rand_block)
DECLARE_UDF(linalg, rand_vector)
DECLARE_UDF(linalg, uniform_vector)
DECLARE_UDF(linalg, normal_vector)
DECLARE_UDF(linalg, matrix_vec_mult_in_mem_2d)
DECLARE_UDF(linalg, matrix_vec_mult_in_mem_1d)
DECLARE_SR_UDF(linalg, row_split)
DECLARE_SR_UDF(linalg, unnest_block)
