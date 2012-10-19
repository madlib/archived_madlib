/* ----------------------------------------------------------------------- *//**
 *
 * @file matrix_agg.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Aggregate matrix from columns: Transition function
 */
DECLARE_UDF(linalg, matrix_agg_transition)

/**
 * @brief Aggregate matrix from columns: Final function
 */
DECLARE_UDF(linalg, matrix_agg_final)

/**
 * @brief Return the column of a matrix
 */
DECLARE_UDF(linalg, matrix_column)
