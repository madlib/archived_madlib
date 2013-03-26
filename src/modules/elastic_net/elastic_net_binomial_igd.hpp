
/**
 * Elastic net regularization for logistic regression using IGD optimizer
 */

/**
 * @brief Logistic regression (incremental gradient): Transition function
 */
DECLARE_UDF(elastic_net, binomial_igd_transition)

/**
 * @brief Logistic regression (incremental gradient): State merge function
 */
DECLARE_UDF(elastic_net, binomial_igd_merge)

/**
 * @brief Logistic regression (incremental gradient): Final function
 */
DECLARE_UDF(elastic_net, binomial_igd_final)

/**
 * @brief Logistic regression (incremental gradient): measure the difference
 * between two consecutive states
 */
DECLARE_UDF(elastic_net, __binomial_igd_state_diff)

/**
 * @brief Logistic regression (incremental gradient): Convert
 *     transition state to result tuple
 */
DECLARE_UDF(elastic_net, __binomial_igd_result)
