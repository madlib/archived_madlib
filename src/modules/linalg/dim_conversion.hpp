/* ----------------------------------------------------------------------- *//**
 *
 * @file dim_conversion.hpp
 *
 *//* ----------------------------------------------------------------------- */

DECLARE_UDF(linalg, array_to_1d)
DECLARE_UDF(linalg, array_to_2d)

/**
 * @brief a row(i) function for 2-D array
 */
DECLARE_UDF(linalg, get_row_from_2d_array)

/**
 * @brief a col(i) function for 2-D array
 */
DECLARE_UDF(linalg, get_col_from_2d_array)

/**
 * @brief deconstruct a 2-D MxN array to a M-row N-col table
 */
DECLARE_SR_UDF(linalg, deconstruct_2d_array)

/**
 * @brief deconstruct lower-triangle of a 2-D array to to a M-col M-row table
 */
DECLARE_SR_UDF(linalg, deconstruct_lower_triangle)
