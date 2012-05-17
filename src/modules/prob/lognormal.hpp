/* ----------------------------------------------------------------------- *//**
 *
 * @file lognormal.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief lognormal cumulative distribution function
 */
DECLARE_UDF(lognormal_cdf)
/**
 * @brief lognormal probability density function
 */
DECLARE_UDF(lognormal_pdf)

/**
 * @brief lognormal quantile function
 */
DECLARE_UDF(lognormal_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double lognormal_CDF(double x, double location, double scale);
double lognormal_PDF(double x, double location, double scale);
double lognormal_QUANTILE(double p, double location, double scale);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
