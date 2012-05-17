/* ----------------------------------------------------------------------- *//**
 *
 * @file linear.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Linear regression: Transition function
 */
DECLARE_UDF(regress, linregr_transition)

/**
 * @brief Linear regression: State merge function
 */
DECLARE_UDF(regress, linregr_merge_states)

/**
 * @brief Linear regression: Final function
 */
DECLARE_UDF(regress, linregr_final)
