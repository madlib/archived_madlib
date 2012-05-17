/* ----------------------------------------------------------------------- *//**
 *
 * @file chi_squared.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief Chi-squared cumulative distribution function
 */
DECLARE_UDF(chi_squared_cdf)
/**
 * @brief Chi-squared probability density function
 */
DECLARE_UDF(chi_squared_pdf)

/**
 * @brief Chi-squared quantile function
 */
DECLARE_UDF(chi_squared_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double chi_squared_CDF(double x, double df);
double chi_squared_PDF(double x, double df);
double chi_squared_QUANTILE(double p, double df);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
