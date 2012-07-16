/* ----------------------------------------------------------------------- *//**
 *
 * @file crf.hpp
 *
 *//* ----------------------------------------------------------------------- */
/**
 * @brief Linear-chain CRF (conjugate-gradient step): Transition function
 */
DECLARE_UDF(crf, lincrf_cg_step_transition)

/**
 * @brief Linear-chain CRF (conjugate-gradient step): State merge function
 */
DECLARE_UDF(crf, lincrf_cg_step_merge_states)

/**
 * @brief Linear-chain CRF (conjugate-gradient step): Final function
 */
DECLARE_UDF(crf, lincrf_cg_step_final)

/**
 * @brief Linear-chain CRF (conjugate-gradient): Difference in log-likelihood
 *     between two transition states
 */
DECLARE_UDF(crf, internal_lincrf_cg_step_distance)

/**
 * @brief Linear-chain CRF (conjugate-gradient): Convert transition state to 
 *     result tuple
 */
DECLARE_UDF(crf, internal_lincrf_cg_result)

