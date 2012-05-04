/* ----------------------------------------------------------------------- *//**
 *
 * @file logistic.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief logistic cumulative distribution function
 */
DECLARE_UDF(logistic_cdf)
/**
 * @brief logistic probability density function
 */
DECLARE_UDF(logistic_pdf)

/**
 * @brief logistic quantile function
 */
DECLARE_UDF(logistic_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double logistic_CDF(double x, double location, double scale);
double logistic_PDF(double x, double location, double scale);
double logistic_QUANTILE(double p, double location, double scale);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
