/* ----------------------------------------------------------------------- *//**
 *
 * @file inverse_gamma.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief inverse_gamma cumulative distribution function
 */
DECLARE_UDF(inverse_gamma_cdf)
/**
 * @brief inverse_gamma probability density function
 */
DECLARE_UDF(inverse_gamma_pdf)

/**
 * @brief inverse_gamma quantile function
 */
DECLARE_UDF(inverse_gamma_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double inverse_gamma_CDF(double x, double shape, double scale);
double inverse_gamma_PDF(double x, double shape, double scale);
double inverse_gamma_QUANTILE(double p, double shape, double scale);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
