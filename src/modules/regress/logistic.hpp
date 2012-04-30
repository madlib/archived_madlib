/* ----------------------------------------------------------------------- *//**
 *
 * @file logistic.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Logistic regression (conjugate-gradient step): Transition function
 */
DECLARE_UDF(regress, logregr_cg_step_transition)

/**
 * @brief Logistic regression (conjugate-gradient step): State merge function
 */
DECLARE_UDF(regress, logregr_cg_step_merge_states)

/**
 * @brief Logistic regression (conjugate-gradient step): Final function
 */
DECLARE_UDF(regress, logregr_cg_step_final)

/**
 * @brief Logistic regression (conjugate-gradient): Difference in log-likelihood
 *     between two transition states
 */
DECLARE_UDF(regress, internal_logregr_cg_step_distance)

/**
 * @brief Logistic regression (conjugate-gradient): Convert transition state to 
 *     result tuple
 */
DECLARE_UDF(regress, internal_logregr_cg_result)


/**
 * @brief Logistic regression (iteratively-reweighted-lest-squares step):
 *     Transition function
 */
DECLARE_UDF(regress, logregr_irls_step_transition)

/**
 * @brief Logistic regression (iteratively-reweighted-lest-squares step):
 *     State merge function
 */
DECLARE_UDF(regress, logregr_irls_step_merge_states)

/**
 * @brief Logistic regression (iteratively-reweighted-lest-squares step):
 *     Final function
 */
DECLARE_UDF(regress, logregr_irls_step_final)

/**
 * @brief Logistic regression (iteratively-reweighted-lest-squares step):
 *     Difference in log-likelihood between two transition states
 */
DECLARE_UDF(regress, internal_logregr_irls_step_distance)

/**
 * @brief Logistic regression (iteratively-reweighted-lest-squares step):
 *     Convert transition state to result tuple
 */
DECLARE_UDF(regress, internal_logregr_irls_result)


/**
 * @brief Logistic regression (incremetal-gradient step): Transition function
 */
DECLARE_UDF(regress, logregr_igd_step_transition)

/**
 * @brief Logistic regression (incremetal-gradient step): State merge function
 */
DECLARE_UDF(regress, logregr_igd_step_merge_states)

/**
 * @brief Logistic regression (incremetal-gradient step): Final function
 */
DECLARE_UDF(regress, logregr_igd_step_final)

/**
 * @brief Logistic regression (incremetal-gradient step): Difference in
 *     log-likelihood between two transition states
 */
DECLARE_UDF(regress, internal_logregr_igd_step_distance)

/**
 * @brief Logistic regression (iteratively-reweighted-lest-squares step):
 *     Convert transition state to result tuple
 */
DECLARE_UDF(regress, internal_logregr_igd_result)
