/* ----------------------------------------------------------------------- *//**
 *
 * @file pareto.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief pareto cumulative distribution function
 */
DECLARE_UDF(pareto_cdf)
/**
 * @brief pareto probability density function
 */
DECLARE_UDF(pareto_pdf)

/**
 * @brief pareto quantile function
 */
DECLARE_UDF(pareto_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double pareto_CDF(double x, double location, double shape);
double pareto_PDF(double x, double location, double shape);
double pareto_QUANTILE(double p, double location, double shape);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
