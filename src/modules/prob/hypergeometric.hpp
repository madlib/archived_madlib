/* ----------------------------------------------------------------------- *//**
 *
 * @file hypergeometric.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief hypergeometric cumulative distribution function
 */
DECLARE_UDF(hypergeometric_cdf)
/**
 * @brief hypergeometric probability mass function
 */
DECLARE_UDF(hypergeometric_pdf)

/**
 * @brief hypergeometric quantile function
 */
DECLARE_UDF(hypergeometric_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double hypergeometric_CDF(double x, int defective, int sample_count, int total);
double hypergeometric_PDF(int x, int defective, int sample_count, int total);
double hypergeometric_QUANTILE(double p, int defective, int sample_count, int total);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
