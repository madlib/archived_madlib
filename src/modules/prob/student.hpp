/* ----------------------------------------------------------------------- *//**
 *
 * @file student.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Student-t cumulative distribution function
 */
DECLARE_UDF(prob, student_t_cdf)


#if !defined(DECLARE_LIBRARY_EXPORTS)

namespace madlib {

namespace modules {

namespace prob {

double studentT_CDF(double t, double nu);

} // namespace prob

} // namespace modules

} // namespace madlib

#endif // !defined(DECLARE_LIBRARY_EXPORTS)
