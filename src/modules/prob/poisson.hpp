/* ----------------------------------------------------------------------- *//**
 *
 * @file poisson.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief poisson cumulative distribution function
 */
DECLARE_UDF(poisson_cdf)
/**
 * @brief poisson probability density function
 */
DECLARE_UDF(poisson_pdf)

/**
 * @brief poisson quantile function
 */
DECLARE_UDF(poisson_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double poisson_CDF(double x, double mean);
double poisson_PDF(double x, double mean);
double poisson_QUANTILE(double p, double mean);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
