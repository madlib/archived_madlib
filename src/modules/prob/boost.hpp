/* ----------------------------------------------------------------------- *//**
 *
 * @file boost.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Chi-squared cumulative distribution function
 */
DECLARE_UDF(prob, chi_squared_cdf)

/**
 * @brief Fisher F cumulative distribution function
 */
DECLARE_UDF(prob, fisher_f_cdf)

/**
 * @brief Normal cumulative distribution function
 */
DECLARE_UDF(prob, normal_cdf)


#if !defined(DECLARE_LIBRARY_EXPORTS)

namespace madlib {

namespace modules {

namespace prob {

double chiSquaredCDF(double t, double nu);
double fisherF_CDF(double t, double df1, double df2);
double normalCDF(double t, double mu = 0, double sigma = 1);

} // namespace prob

} // namespace modules

} // namespace madlib

#endif // !defined(DECLARE_LIBRARY_EXPORTS)
