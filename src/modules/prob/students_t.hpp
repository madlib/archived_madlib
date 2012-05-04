/* ----------------------------------------------------------------------- *//**
 *
 * @file students_t.hpp 
 *
 *//* ----------------------------------------------------------------------- */ 

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)
/**
 * @brief students_t cumulative distribution function
 */
DECLARE_UDF(students_t_cdf)
/**
 * @brief students_t probability density function
 */
DECLARE_UDF(students_t_pdf)

/**
 * @brief students_t quantile function
 */
DECLARE_UDF(students_t_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double students_t_CDF(double x, double df);
double students_t_PDF(double x, double df);
double students_t_QUANTILE(double p, double df);

#endif // !defined(DECLARE_LIBRARY_EXPORTS) 
#endif // workaround for Doxygen
