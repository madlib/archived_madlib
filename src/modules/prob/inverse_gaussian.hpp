/* ----------------------------------------------------------------------- *//**
 *
 * @file inverse_gaussian.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief inverse_gaussian cumulative distribution function
 */
DECLARE_UDF(inverse_gaussian_cdf)
/**
 * @brief inverse_gaussian probability density function
 */
DECLARE_UDF(inverse_gaussian_pdf)

/**
 * @brief inverse_gaussian quantile function
 */
DECLARE_UDF(inverse_gaussian_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double inverse_gaussian_CDF(double x, double mean, double scale);
double inverse_gaussian_PDF(double x, double mean, double scale);
double inverse_gaussian_QUANTILE(double p, double mean, double scale);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
