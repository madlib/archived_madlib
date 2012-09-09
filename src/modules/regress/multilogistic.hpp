/* ----------------------------------------------------------------------- *//**
 *
 * @file multilogistic.hpp
 *
 *//* ----------------------------------------------------------------------- */


/**
 * @brief Multi Logistic regression (iteratively-reweighted-lest-squares step):
 *     Transition function
 */
DECLARE_UDF(regress, mlogregr_irls_step_transition)

/**
 * @brief Multi Logistic regression (iteratively-reweighted-lest-squares step):
 *     State merge function
 */
DECLARE_UDF(regress, mlogregr_irls_step_merge_states)

/**
 * @brief Multi Logistic regression (iteratively-reweighted-lest-squares step):
 *     Final function
 */
DECLARE_UDF(regress, mlogregr_irls_step_final)

/**
 * @brief Multi Logistic regression (iteratively-reweighted-lest-squares step):
 *     Difference in log-likelihood between two transition states
 */
DECLARE_UDF(regress, internal_mlogregr_irls_step_distance)

/**
 * @brief Multi Logistic regression (iteratively-reweighted-lest-squares step):
 *     Convert transition state to result tuple
 */
DECLARE_UDF(regress, internal_mlogregr_irls_result)

//Robust variance calculation functions
/**
 * @brief Robust Variance Logistic regression step: Transition function
 */
DECLARE_UDF(regress, mlogregr_robust_step_transition)

/**
 * @brief Robust Variance Logistic regression step: State merge function
 */
DECLARE_UDF(regress, mlogregr_robust_step_merge_states)

/**
 * @brief Robust Variance Logistic regression step: Final function
 */
DECLARE_UDF(regress, mlogregr_robust_step_final)


