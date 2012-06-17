/* ----------------------------------------------------------------------- *//**
 *
 * @file nlp.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Logistic nlpion (conjugate-gradient step): Transition function
 */
DECLARE_UDF(nlp, nlp_gradient_step_transition)

/**
 * @brief Logistic nlpion (conjugate-gradient step): State merge function
 */
DECLARE_UDF(nlp, nlp_gradient_step_merge_states)

/**
 * @brief Logistic nlpion (conjugate-gradient step): Final function
 */
DECLARE_UDF(nlp, nlp_gradient_step_final)
