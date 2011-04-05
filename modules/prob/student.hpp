/* ----------------------------------------------------------------------- *//**
 *
 * @file student.hpp
 *
 * @brief Evaluate the Student-T distribution function.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef STUDENT_H
#define STUDENT_H

#include <madlib/modules/modules.hpp>

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
