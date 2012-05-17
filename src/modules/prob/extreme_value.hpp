/* ----------------------------------------------------------------------- *//**
 *
 * @file extreme_value.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief extreme_value cumulative distribution function
 */
DECLARE_UDF(extreme_value_cdf)
/**
 * @brief extreme_value probability density function
 */
DECLARE_UDF(extreme_value_pdf)

/**
 * @brief extreme_value quantile function
 */
DECLARE_UDF(extreme_value_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double extreme_value_CDF(double x, double location, double scale);
double extreme_value_PDF(double x, double location, double scale);
double extreme_value_QUANTILE(double p, double location, double scale);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
