/* ----------------------------------------------------------------------- *//**
 *
 * @file multi_response_glm.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Multivariate response Generalized linear model: Transition function
 */
DECLARE_UDF(glm, multi_response_glm_multinom_logit_transition)

/**
 * @brief Multivariate response Generalized linear model: State merge function
 */
DECLARE_UDF(glm, multi_response_glm_merge_states)

/**
 * @brief Multivariate response Generalized linear model: Final function
 */
DECLARE_UDF(glm, multi_response_glm_final)

/**
 * @brief Multivariate response Generalized linear model: Result function
 */
DECLARE_UDF(glm, multi_response_glm_result_z_stats)

/**
 * @brief Multivariate response Generalized linear model: Distance function
 */
DECLARE_UDF(glm, multi_response_glm_loglik_diff)
