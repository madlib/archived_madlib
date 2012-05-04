/* ----------------------------------------------------------------------- *//**
 *
 * @file non_central_f.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief non_central_f cumulative distribution function
 */
DECLARE_UDF(non_central_f_cdf)
/**
 * @brief non_central_f probability density function
 */
DECLARE_UDF(non_central_f_pdf)

/**
 * @brief non_central_f quantile function
 */
DECLARE_UDF(non_central_f_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double non_central_f_CDF(double x, double df1, double df2, double non_centrality);
double non_central_f_PDF(double x, double df1, double df2, double non_centrality);
double non_central_f_QUANTILE(double p, double df1, double df2, double non_centrality);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
