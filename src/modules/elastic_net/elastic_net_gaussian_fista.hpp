
/**
 * Elastic net regulation for linear regression using FISTA optimizer
 */

/**
 * @brief Linear regression (incremental gradient): Transition function
 */
DECLARE_UDF(elastic_net, gaussian_fista_transition)

/**
 * @brief Linear regression (incremental gradient): State merge function
 */
DECLARE_UDF(elastic_net, gaussian_fista_merge)

/**
 * @brief Linear regression (incremental gradient): Final function
 */
DECLARE_UDF(elastic_net, gaussian_fista_final)

/**
 * @brief Linear regression (incremental gradient): Difference in
 *     log-likelihood between two transition states
 */
DECLARE_UDF(elastic_net, __gaussian_fista_state_diff)

/**
 * @brief Linear regression (incremental gradient): Convert
 *     transition state to result tuple
 */
DECLARE_UDF(elastic_net, __gaussian_fista_result)

// /**
//  * @brief (incremental gradient): Prediction
//  */
// DECLARE_UDF(elastic_net, gaussian_igd_predict)
