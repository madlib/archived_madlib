/* ----------------------------------------------------------------------- *//**
 *
 * @file cauchy.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief cauchy cumulative distribution function
 */
DECLARE_UDF(cauchy_cdf)
/**
 * @brief cauchy probability density function
 */
DECLARE_UDF(cauchy_pdf)

/**
 * @brief cauchy quantile function
 */
DECLARE_UDF(cauchy_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double cauchy_CDF(double x, double location, double scale);
double cauchy_PDF(double x, double location, double scale);
double cauchy_QUANTILE(double p, double location, double scale);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
