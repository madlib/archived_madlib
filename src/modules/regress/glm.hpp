/* ----------------------------------------------------------------------- *//**
 *
 * @file glm.hpp
 *
 *//* ----------------------------------------------------------------------- */


/**
 * @brief glm regression (iteratively-reweighted-lest-squares step):
 *     Transition function
 */
//DECLARE_UDF(regress, glmregr_irls_step_transition)

/**
 * @brief Logistic regression (iteratively-reweighted-lest-squares step):
 *     State merge function
 */
//DECLARE_UDF(regress, glmregr_irls_step_merge_states)

/**
 * @brief Logistic regression (iteratively-reweighted-lest-squares step):
 *     Final function
 */
//DECLARE_UDF(regress, glmregr_irls_step_final)

/**
 * @brief Logistic regression (iteratively-reweighted-lest-squares step):
 *     Difference in log-likelihood between two transition states
 */
//DECLARE_UDF(regress, internal_glmregr_irls_step_distance)

/**
 * @brief Logistic regression (iteratively-reweighted-lest-squares step):
 *     Convert transition state to result tuple
 */
//DECLARE_UDF(regress, internal_glmregr_irls_result)

DECLARE_UDF(regress, glm)
DECLARE_UDF(regress, glm2)

/**
 * @brief Gaussian family (conjugate-gradient step): Final function
 */
DECLARE_UDF(regress, gaussian)
DECLARE_UDF(regress, binomial)
