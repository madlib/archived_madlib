/* ----------------------------------------------------------------------- *//**
 *
 * @file student.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if processed as a stand-alone.
#if !defined(_DOXYGEN) || defined(MADLIB_PROB_STUDENT_CPP)

/**
 * @brief Student-t cumulative distribution function
 */
DECLARE_UDF(student_t_cdf)

double studentT_CDF(int64_t nu, double t);

#endif // workaround for Doxygen
