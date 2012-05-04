/* ----------------------------------------------------------------------- *//**
 *
 * @file beta.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief beta cumulative distribution function
 */
DECLARE_UDF(beta_cdf)
/**
 * @brief beta probability density function
 */
DECLARE_UDF(beta_pdf)

/**
 * @brief beta quantile function
 */
DECLARE_UDF(beta_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double beta_CDF(double x, double alpha, double beta);
double beta_PDF(double x, double alpha, double beta);
double beta_QUANTILE(double p, double alpha, double beta);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
