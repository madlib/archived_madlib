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

double studentT_cdf(int64_t nu, double t);

/**
 * Expose studentT_cdf as a user-defined function.
 */
struct student_t_cdf {
    AnyValue operator()(AnyValue::iterator arg) const {
        int64_t nu = *arg;
        double t = *(++arg);
        
        /* We want to ensure nu > 0 */
        if (nu <= 0)
            throw std::domain_error("Student-t distribution undefined for "
                "degree of freedom <= 0");

        return studentT_cdf(nu, t);    
    }
};

} // namespace prob

} // namespace modules

} // namespace regress

#endif
