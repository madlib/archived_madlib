/* ----------------------------------------------------------------------- *//**
 *
 * @file ridge_newton.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Ridge regression (Newton's method): Transition function
 */
DECLARE_UDF(convex, ridge_newton_transition)

/**
 * @brief Ridge regression (Newton's method): State merge function
 */
DECLARE_UDF(convex, ridge_newton_merge)

/**
 * @brief Ridge regression (Newton's method): Final function
 */
DECLARE_UDF(convex, ridge_newton_final)

/**
 * @brief Ridge regression (Newton's method): Convert
 *     transition state to result tuple
 */
DECLARE_UDF(convex, internal_ridge_newton_result)

/**
 * @brief Ridge regression (Newton's method): Prediction
 */
DECLARE_UDF(convex, ridge_newton_predict)

