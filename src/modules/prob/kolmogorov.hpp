/* ----------------------------------------------------------------------- *//**
 *
 * @file kolmogorov.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Kolmogorov cumulative distribution function
 */
DECLARE_UDF(prob, kolmogorov_cdf)


#if !defined(DECLARE_LIBRARY_EXPORTS)

namespace madlib {

namespace modules {

namespace prob {

double kolmogorovCDF(double t);

} // namespace prob

} // namespace modules

} // namespace madlib

#endif // !defined(DECLARE_LIBRARY_EXPORTS)
