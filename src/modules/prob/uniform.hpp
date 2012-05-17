/* ----------------------------------------------------------------------- *//**
 *
 * @file uniform.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief uniform cumulative distribution function
 */
DECLARE_UDF(uniform_cdf)
/**
 * @brief uniform probability density function
 */
DECLARE_UDF(uniform_pdf)

/**
 * @brief uniform quantile function
 */
DECLARE_UDF(uniform_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double uniform_CDF(double x, double lower, double upper);
double uniform_PDF(double x, double lower, double upper);
double uniform_QUANTILE(double p, double lower, double upper);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
