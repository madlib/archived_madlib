/* ----------------------------------------------------------------------- *//**
 *
 * @file ordinal.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Ordinal linear model: Transition function
 */
DECLARE_UDF(glm, ordinal_logit_transition)
DECLARE_UDF(glm, ordinal_probit_transition)

/**
 * @brief Ordinal linear model: State merge function
 */
DECLARE_UDF(glm, ordinal_merge_states)

/**
 * @brief Ordinal linear model: Final function
 */
DECLARE_UDF(glm, ordinal_final)

/**
 * @brief Ordinal linear model: Result function
 */
DECLARE_UDF(glm, ordinal_result)

/**
 * @brief Ordinal linear model: Distance function
 */
DECLARE_UDF(glm, ordinal_loglik_diff)
