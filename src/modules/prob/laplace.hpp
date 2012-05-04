/* ----------------------------------------------------------------------- *//**
 *
 * @file laplace.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief laplace cumulative distribution function
 */
DECLARE_UDF(laplace_cdf)
/**
 * @brief laplace probability density function
 */
DECLARE_UDF(laplace_pdf)

/**
 * @brief laplace quantile function
 */
DECLARE_UDF(laplace_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double laplace_CDF(double x, double location, double scale);
double laplace_PDF(double x, double location, double scale);
double laplace_QUANTILE(double p, double location, double scale);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
