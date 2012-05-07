/* ----------------------------------------------------------------------- *//**
 *
 * @file negative_binomial.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief negative_binomial cumulative distribution function
 */
DECLARE_UDF(negative_binomial_cdf)
/**
 * @brief negative_binomial probability density function
 */
DECLARE_UDF(negative_binomial_pdf)

/**
 * @brief negative_binomial quantile function
 */
DECLARE_UDF(negative_binomial_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double negative_binomial_CDF(double x, double successes, double succ_prob);
double negative_binomial_PDF(double x, double successes, double succ_prob);
double negative_binomial_QUANTILE(double p, double successes, double succ_prob);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
