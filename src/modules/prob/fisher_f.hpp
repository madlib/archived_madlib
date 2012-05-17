/* ----------------------------------------------------------------------- *//**
 *
 * @file fisher_f.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief Fisher-f cumulative distribution function
 */
DECLARE_UDF(fisher_f_cdf)
/**
 * @brief Fisher-f probability density function
 */
DECLARE_UDF(fisher_f_pdf)

/**
 * @brief Fisher-f quantile function
 */
DECLARE_UDF(fisher_f_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double fisher_f_CDF(double x, double df1, double df2);
double fisher_f_PDF(double x, double df1, double df2);
double fisher_f_QUANTILE(double p, double df1, double df2);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
