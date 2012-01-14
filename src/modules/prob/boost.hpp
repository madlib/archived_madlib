/* ----------------------------------------------------------------------- *//**
 *
 * @file boost.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if processed as a stand-alone.
#if !defined(_DOXYGEN) || defined(MADLIB_PROB_BOOST_CPP)

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
