/* ----------------------------------------------------------------------- *//**
 *
 * @file geometric.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief geometric cumulative distribution function
 */
DECLARE_UDF(geometric_cdf)
/**
 * @brief geometric probability density function
 */
DECLARE_UDF(geometric_pdf)

/**
 * @brief geometric quantile function
 */
DECLARE_UDF(geometric_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double geometric_CDF(double x, double suc_prob);
double geometric_PDF(double x, double suc_prob);
double geometric_QUANTILE(double p, double suc_prob);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
