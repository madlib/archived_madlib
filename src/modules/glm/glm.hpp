/* ----------------------------------------------------------------------- *//**
 *
 * @file glm.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Generalized linear model: Transition function
 */
DECLARE_UDF(glm, glm_poisson_log_transition)
DECLARE_UDF(glm, glm_poisson_identity_transition)
DECLARE_UDF(glm, glm_poisson_sqrt_transition)

/**
 * @brief Generalized linear model: State merge function
 */
DECLARE_UDF(glm, glm_merge_states)

/**
 * @brief Generalized linear model: Final function
 */
DECLARE_UDF(glm, glm_final)

/**
 * @brief Generalized linear model: Result function
 */
DECLARE_UDF(glm, glm_result)

/**
 * @brief Generalized linear model: Distance function
 */
DECLARE_UDF(glm, glm_loglik_diff)
