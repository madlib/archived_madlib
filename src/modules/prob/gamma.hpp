/* ----------------------------------------------------------------------- *//**
 *
 * @file gamma.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief gamma cumulative distribution function
 */
DECLARE_UDF(gamma_cdf)
/**
 * @brief gamma probability density function
 */
DECLARE_UDF(gamma_pdf)

/**
 * @brief gamma quantile function
 */
DECLARE_UDF(gamma_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double gamma_CDF(double x, double shape, double scale);
double gamma_PDF(double x, double shape, double scale);
double gamma_QUANTILE(double p, double shape, double scale);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
