/* ----------------------------------------------------------------------- *//**
 *
 * @file logistic.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Logistic regression (conjugate-gradient step): Transition function
 */
DECLARE_UDF(hello_world, logregr_simple_step_transition)

/**
 * @brief Logistic regression (conjugate-gradient step): State merge function
 */
DECLARE_UDF(hello_world, logregr_simple_step_merge_states)

/**
 * @brief Logistic regression (conjugate-gradient step): Final function
 */
DECLARE_UDF(hello_world, logregr_simple_step_final)

/**
 * @brief Logistic regression (conjugate-gradient): Difference in log-likelihood
 *     between two transition states
 */
DECLARE_UDF(hello_world, internal_logregr_simple_step_distance)

/**
 * @brief Logistic regression (conjugate-gradient): Convert transition state to 
 *     result tuple
 */
DECLARE_UDF(hello_world, internal_logregr_simple_result)
