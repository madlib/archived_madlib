
/**
 * Utility functions for elastic net
 */

DECLARE_UDF(elastic_net, __elastic_net_gaussian_predict)

// compute the True/False prediction
DECLARE_UDF(elastic_net, __elastic_net_binomial_predict)

// compute probabilities for class True
DECLARE_UDF(elastic_net, __elastic_net_binomial_prob)

// compute log-likelihood
DECLARE_UDF(elastic_net, __elastic_net_binomial_loglikelihood)

