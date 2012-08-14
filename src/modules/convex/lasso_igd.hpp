/* ----------------------------------------------------------------------- *//**
 *
 * @file lasso_igd.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Logistic regression (incremental gradient): Transition function
 */
DECLARE_UDF(convex, lasso_igd_transition)

/**
 * @brief Logistic regression (incremental gradient): State merge function
 */
DECLARE_UDF(convex, lasso_igd_merge)

/**
 * @brief Logistic regression (incremental gradient): Final function
 */
DECLARE_UDF(convex, lasso_igd_final)

/**
 * @brief Logistic regression (incremental gradient): Difference in
 *     log-likelihood between two transition states
 */
DECLARE_UDF(convex, internal_lasso_igd_distance)

/**
 * @brief Logistic regression (incremental gradient): Convert
 *     transition state to result tuple
 */
DECLARE_UDF(convex, internal_lasso_igd_result)

/**
 * @brief Logistic regression (incremental gradient): Prediction
 */
DECLARE_UDF(convex, lasso_igd_predict)

