/* ----------------------------------------------------------------------- *//**
 *
 * @file linear_svm_igd.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Linear support vector machine (incremental gradient): Transition function
 */
DECLARE_UDF(convex, linear_svm_igd_transition)

/**
 * @brief Linear support vector machine (incremental gradient): State merge function
 */
DECLARE_UDF(convex, linear_svm_igd_merge)

/**
 * @brief Linear support vector machine (incremental gradient): Final function
 */
DECLARE_UDF(convex, linear_svm_igd_final)

/**
 * @brief Linear support vector machine (incremental gradient): Difference in
 *     log-likelihood between two transition states
 */
DECLARE_UDF(convex, internal_linear_svm_igd_distance)

/**
 * @brief Linear support vector machine (incremental gradient): Convert
 *     transition state to result tuple
 */
DECLARE_UDF(convex, internal_linear_svm_igd_result)


DECLARE_UDF(convex, structure_svm_transition)
DECLARE_UDF(convex, structure_svm_merge)
DECLARE_UDF(convex, structure_svm_final)
DECLARE_UDF(convex, structure_svm_predict)
DECLARE_UDF(convex, internal_structure_svm_result)

