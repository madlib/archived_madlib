/* ----------------------------------------------------------------------- *//**
 *
 * @file mlp_igd.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Multi-layer perceptron (incremental gradient): Transition function
 */
DECLARE_UDF(convex, mlp_igd_transition)

/**
 * @brief Multi-layer perceptron (incremental gradient): State merge function
 */
DECLARE_UDF(convex, mlp_igd_merge)

/**
 * @brief Multi-layer perceptron (incremental gradient): Final function
 */
DECLARE_UDF(convex, mlp_igd_final)

/**
 * @brief Multi-layer perceptron (incremental gradient): Difference in
 *     log-likelihood between two transition states
 */
DECLARE_UDF(convex, internal_mlp_igd_distance)

/**
 * @brief Multi-layer perceptron (incremental gradient): Convert
 *     transition state to result tuple
 */
DECLARE_UDF(convex, internal_mlp_igd_result)

