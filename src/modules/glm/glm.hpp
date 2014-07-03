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

DECLARE_UDF(glm, glm_gaussian_log_transition)
DECLARE_UDF(glm, glm_gaussian_identity_transition)
DECLARE_UDF(glm, glm_gaussian_inverse_transition)

DECLARE_UDF(glm, glm_gamma_log_transition)
DECLARE_UDF(glm, glm_gamma_identity_transition)
DECLARE_UDF(glm, glm_gamma_inverse_transition)

DECLARE_UDF(glm, glm_inverse_gaussian_log_transition)
DECLARE_UDF(glm, glm_inverse_gaussian_identity_transition)
DECLARE_UDF(glm, glm_inverse_gaussian_inverse_transition)
DECLARE_UDF(glm, glm_inverse_gaussian_sqr_inverse_transition)

DECLARE_UDF(glm, glm_binomial_probit_transition)
DECLARE_UDF(glm, glm_binomial_logit_transition)

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
DECLARE_UDF(glm, glm_result_z_stats)
DECLARE_UDF(glm, glm_result_t_stats)

/**
 * @brief Generalized linear model: Distance function
 */
DECLARE_UDF(glm, glm_loglik_diff)

/**
 * @brief Generalized linear model: Prediction function
 */
DECLARE_UDF(glm, glm_predict)
DECLARE_UDF(glm, glm_predict_binomial)
DECLARE_UDF(glm, glm_predict_poisson)
