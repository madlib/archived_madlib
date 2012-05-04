/* ----------------------------------------------------------------------- *//**
 *
 * @file non_central_chi_squared.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief non_central_chi_squared cumulative distribution function
 */
DECLARE_UDF(non_central_chi_squared_cdf)
/**
 * @brief non_central_chi_squared probability density function
 */
DECLARE_UDF(non_central_chi_squared_pdf)

/**
 * @brief non_central_chi_squared quantile function
 */
DECLARE_UDF(non_central_chi_squared_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double non_central_chi_squared_CDF(double x, double df, double non_centrality);
double non_central_chi_squared_PDF(double x, double df, double non_centrality);
double non_central_chi_squared_QUANTILE(double p, double df, double non_centrality);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
