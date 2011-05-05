/* ----------------------------------------------------------------------- *//**
 *
 * @file student.hpp
 *
 * @brief Evaluate the Student-T distribution function.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_PROB_STUDENT_H
#define MADLIB_PROB_STUDENT_H

#include <madlib/modules/common.hpp>

namespace madlib {

namespace modules {

namespace prob {

/**
 * C/C++ interface to Student-t CDF
 */
double studentT_cdf(int64_t nu, double t);

/**
 * In-DB interface to Student-t CDF
 */
AnyValue student_t_cdf(AbstractDBInterface &db, AnyValue args);

} // namespace prob

} // namespace modules

} // namespace regress

#endif
