/* ----------------------------------------------------------------------- *//**
 *
 * @file linear_svm_cg.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Linear support vector machine (conjugate gradient): Transition function
 */
DECLARE_UDF(convex, linear_svm_cg_transition)

/**
 * @brief Linear support vector machine (conjugate gradient): State merge function
 */
DECLARE_UDF(convex, linear_svm_cg_merge)

/**
 * @brief Linear support vector machine (conjugate gradient): Final function
 */
DECLARE_UDF(convex, linear_svm_cg_final)

/**
 * @brief Linear support vector machine (conjugate gradient): Difference in
 *     log-likelihood between two transition states
 */
DECLARE_UDF(convex, internal_linear_svm_cg_distance)

/**
 * @brief Linear support vector machine (conjugate gradient): Convert
 *     transition state to result tuple
 */
DECLARE_UDF(convex, internal_linear_svm_cg_result)

/**
 * @brief Linear support vector machine (conjugate gradient): Prediction
 */
DECLARE_UDF(convex, linear_svm_cg_predict)

