/* ----------------------------------------------------------------------- *//**
 *
 * @file normal.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief normal cumulative distribution function
 */
DECLARE_UDF(normal_cdf)
/**
 * @brief normal probability density function
 */
DECLARE_UDF(normal_pdf)

/**
 * @brief normal quantile function
 */
DECLARE_UDF(normal_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double normal_CDF(double x, double mean = 0, double sd = 1);
double normal_PDF(double x, double mean = 0, double sd = 1);
double normal_QUANTILE(double p, double mean = 0, double sd = 1);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
