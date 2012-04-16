/* ----------------------------------------------------------------------- *//**
 *
 * @file boost.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)

/**
 * @brief Chi-squared cumulative distribution function
 */
DECLARE_UDF(chi_squared_cdf)

/**
 * @brief Fisher F cumulative distribution function
 */
DECLARE_UDF(fisher_f_cdf)

/**
 * @brief Normal cumulative distribution function
 */
DECLARE_UDF(normal_cdf)

/**
 * @brief Normal probability density function
 */
DECLARE_UDF(normal_pdf)

#if !defined(DECLARE_LIBRARY_EXPORTS)

double chiSquaredCDF(double t, double nu);
double fisherF_CDF(double t, double df1, double df2);
double normalCDF(double t, double mu = 0, double sigma = 1);
double normalPDF(double t, double mu = 0.0, double sigma = 1.0);
#endif // !defined(DECLARE_LIBRARY_EXPORTS)

#endif // workaround for Doxygen
