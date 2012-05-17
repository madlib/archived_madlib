/* ----------------------------------------------------------------------- *//**
 *
 * @file triangular.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief triangular cumulative distribution function
 */
DECLARE_UDF(triangular_cdf)
/**
 * @brief triangular probability density function
 */
DECLARE_UDF(triangular_pdf)

/**
 * @brief triangular quantile function
 */
DECLARE_UDF(triangular_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double triangular_CDF(double x, double lower, double mode, double upper);
double triangular_PDF(double x, double lower, double mode, double upper);
double triangular_QUANTILE(double p, double lower, double mode, double upper);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
