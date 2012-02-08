/* ----------------------------------------------------------------------- *//**
 *
 * @file student.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)

/**
 * @brief Student-t cumulative distribution function
 */
DECLARE_UDF(student_t_cdf)

double studentT_CDF(int64_t nu, double t);

#endif // workaround for Doxygen
