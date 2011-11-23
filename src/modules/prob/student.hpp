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

AnyType students_t_cdf(AbstractDBInterface &db, AnyType args);
AnyType students_t_pdf(AbstractDBInterface &db, AnyType args);
AnyType studentv_t_quantile(AbstractDBInterface &db, AnyType args);

} // namespace prob

} // namespace modules

} // namespace regress

#endif
