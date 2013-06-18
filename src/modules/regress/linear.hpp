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

/**
 * @brief Test for hetereskedasticity: Transition function
 */
DECLARE_UDF(regress, hetero_linregr_transition)

/**
 * @brief Test for hetereskedasticity: State merge function
 */
DECLARE_UDF(regress, hetero_linregr_merge_states)

/**
 * @brief Test for hetereskedasticity: Final function
 */
DECLARE_UDF(regress, hetero_linregr_final)


/**
 * @brief Robust Linear regression: Transition function
 */
DECLARE_UDF(regress, robust_linregr_transition)

/**
 * @brief Robust Linear regression: State merge function
 */
DECLARE_UDF(regress, robust_linregr_merge_states)

/**
 * @brief Robust Linear regression: Final function
 */
DECLARE_UDF(regress, robust_linregr_final)
