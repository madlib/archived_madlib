/* ----------------------------------------------------------------------- *//**
 *
 * @file linear_crf.hpp
 *
 *//* ----------------------------------------------------------------------- */
/**
 * @brief Linear-chain CRF (L-BFGS step): Transition function
 */
DECLARE_UDF(crf, lincrf_lbfgs_step_transition)

/**
 * @brief Linear-chain CRF (L-BFGS step): State merge function
 */
DECLARE_UDF(crf, lincrf_lbfgs_step_merge_states)

/**
 * @brief Linear-chain CRF (L-BFGS step): Final function
 */
DECLARE_UDF(crf, lincrf_lbfgs_step_final)

/**
 * @brief Linear-chain CRF (L-BFGS): Difference in log-likelihood
 *     between two transition states
 */
DECLARE_UDF(crf, internal_lincrf_lbfgs_converge)

/**
 * @brief Linear-chain CRF (L-BFGS): Convert transition state to 
 *     result tuple
 */
DECLARE_UDF(crf, internal_lincrf_lbfgs_result)

