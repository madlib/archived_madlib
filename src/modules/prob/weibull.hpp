/* ----------------------------------------------------------------------- *//**
 *
 * @file weibull.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief weibull cumulative distribution function
 */
DECLARE_UDF(weibull_cdf)
/**
 * @brief weibull probability density function
 */
DECLARE_UDF(weibull_pdf)

/**
 * @brief weibull quantile function
 */
DECLARE_UDF(weibull_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double weibull_CDF(double x, double shape, double scale);
double weibull_PDF(double x, double shape, double scale);
double weibull_QUANTILE(double p, double shape, double scale);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
