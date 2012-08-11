/* ----------------------------------------------------------------------- *//**
 *
 * @file logit_newton.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Logistic regression (Newton's method): Transition function
 */
DECLARE_UDF(convex, logit_newton_transition)

/**
 * @brief Logistic regression (Newton's method): State merge function
 */
DECLARE_UDF(convex, logit_newton_merge)

/**
 * @brief Logistic regression (Newton's method): Final function
 */
DECLARE_UDF(convex, logit_newton_final)

/**
 * @brief Logistic regression (Newton's method): Difference in
 *     log-likelihood between two transition states
 */
DECLARE_UDF(convex, internal_logit_newton_distance)

/**
 * @brief Logistic regression (Newton's method): Convert
 *     transition state to result tuple
 */
DECLARE_UDF(convex, internal_logit_newton_result)

/**
 * @brief Logistic regression (Newton's method): Prediction
 */
DECLARE_UDF(convex, logit_newton_predict)

