/* ----------------------------------------------------------------------- *//**
 *
 * @file exponential.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief exponential cumulative distribution function
 */
DECLARE_UDF(exponential_cdf)
/**
 * @brief exponential probability density function
 */
DECLARE_UDF(exponential_pdf)

/**
 * @brief exponential quantile function
 */
DECLARE_UDF(exponential_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double exponential_CDF(double x, double rate);
double exponential_PDF(double x, double rate);
double exponential_QUANTILE(double p, double rate);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
