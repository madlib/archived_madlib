/* ----------------------------------------------------------------------- *//**
 *
 * @file student.hpp
 *
 * @brief Evaluate the Student-T distribution function.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_PROB_STUDENT_H
#define MADLIB_PROB_STUDENT_H

#include <modules/common.hpp>

namespace madlib {

namespace modules {

namespace prob {

double studentT_cdf(int64_t nu, double t);

AnyType student_t_cdf(AbstractDBInterface &db, AnyType args);

} // namespace prob

} // namespace modules

} // namespace regress

#endif
