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
 * @brief Normal cumulative distribution function
 */
DECLARE_UDF(normal_cdf)


double chiSquaredCDF(int64_t nu, double t);
double normalCDF(double t);

#endif // workaround for Doxygen
