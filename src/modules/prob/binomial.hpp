/* ----------------------------------------------------------------------- *//**
 *
 * @file binomial.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief binomial cumulative distribution function
 */
DECLARE_UDF(binomial_cdf)
/**
 * @brief binomial probability mass function
 */
DECLARE_UDF(binomial_pdf)

/**
 * @brief binomial quantile function
 */
DECLARE_UDF(binomial_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double binomial_CDF(double x, int trials, double succ_prob);
double binomial_PDF(int x, int trials, double succ_prob);
double binomial_QUANTILE(double p, int trials, double succ_prob);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
