/* ----------------------------------------------------------------------- *//**
 *
 * @file bernoulli.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief bernoulli cumulative distribution function
 */
DECLARE_UDF(bernoulli_cdf)
/**
 * @brief bernoulli probability density function
 */
DECLARE_UDF(bernoulli_pdf)

/**
 * @brief bernoulli quantile function
 */
DECLARE_UDF(bernoulli_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double bernoulli_CDF(double x, double succ_prob);
double bernoulli_PDF(double x, double succ_prob);
double bernoulli_QUANTILE(double p, double succ_prob);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
