/* ----------------------------------------------------------------------- *//**
 *
 * @file crf.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Logistic crfion (conjugate-gradient step): Transition function
 */
DECLARE_UDF(crf, crf_gradient_step_transition)

/**
 * @brief Logistic crfion (conjugate-gradient step): State merge function
 */
DECLARE_UDF(crf, crf_gradient_step_merge_states)

/**
 * @brief Logistic crfion (conjugate-gradient step): Final function
 */
DECLARE_UDF(crf, crf_gradient_step_final)
