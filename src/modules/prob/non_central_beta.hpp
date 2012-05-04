/* ----------------------------------------------------------------------- *//**
 *
 * @file non_central_beta.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief non_central_beta cumulative distribution function
 */
DECLARE_UDF(non_central_beta_cdf)
/**
 * @brief non_central_beta probability density function
 */
DECLARE_UDF(non_central_beta_pdf)

/**
 * @brief non_central_beta quantile function
 */
DECLARE_UDF(non_central_beta_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double non_central_beta_CDF(double x, double alpha, double beta, double lambda);
double non_central_beta_PDF(double x, double alpha, double beta, double lambda);
double non_central_beta_QUANTILE(double p, double alpha, double beta, double lambda);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
