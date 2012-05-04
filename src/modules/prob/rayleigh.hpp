/* ----------------------------------------------------------------------- *//**
 *
 * @file rayleigh.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief rayleigh cumulative distribution function
 */
DECLARE_UDF(rayleigh_cdf)
/**
 * @brief rayleigh probability density function
 */
DECLARE_UDF(rayleigh_pdf)

/**
 * @brief rayleigh quantile function
 */
DECLARE_UDF(rayleigh_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double rayleigh_CDF(double x, double sigma);
double rayleigh_PDF(double x, double sigma);
double rayleigh_QUANTILE(double p, double sigma);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
