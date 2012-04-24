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
 * @brief Chi-squared distribution probability density function
 */
DECLARE_UDF(chi_squared_pdf)

/**
 * @brief Chi-squared distribution quantile function
 */
DECLARE_UDF(chi_squared_quantile)

/**
 * @brief Fisher F cumulative distribution function
 */
DECLARE_UDF(fisher_f_cdf)

/**
 * @brief Fisher F probability density function
 */
DECLARE_UDF(fisher_f_pdf)

/**
 * @brief Fisher F quantile function
 */
DECLARE_UDF(fisher_f_quantile)

/**
 * @brief Normal cumulative distribution function
 */
DECLARE_UDF(normal_cdf)

/**
 * @brief Normal probability density function
 */
DECLARE_UDF(normal_pdf)

/**
 * @brief Normal quantile function
 */
DECLARE_UDF(normal_quantile)

/**
 * @brief Beta cumulative distribution function
 */
DECLARE_UDF(beta_cdf)

/**
 * @brief Beta probability density function
 */
DECLARE_UDF(beta_pdf)

/**
 * @brief Fisher F quantile function
 */
DECLARE_UDF(beta_quantile)

#if !defined(DECLARE_LIBRARY_EXPORTS)
double chiSquaredCDF(double t, double nu);
double chiSquaredPDF(double t, double nu);
double chiSquaredQuantile(double t, double nu);
double fisherF_CDF(double t, double df1, double df2);
double fisherF_PDF(double t, double df1, double df2);
double fisherF_Quantile(double t, double df1, double df2);
double normalCDF(double t, double mu = 0, double sigma = 1);
double normalPDF(double t, double mu = 0.0, double sigma = 1.0);
double normalQuantile(double t, double mu = 0.0, double sigma = 1.0);
double beta_CDF(double t, double alpha, double beta);
double beta_PDF(double t, double alpha, double beta);
double beta_Quantile(double t, double alpha, double beta);
#endif // !defined(DECLARE_LIBRARY_EXPORTS)

#endif // workaround for Doxygen
