/* ----------------------------------------------------------------------- *//**
 *
 * @file student.cpp
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#include "student.hpp"

namespace madlib {

namespace modules {

namespace prob {

/**
 * @brief Student-t cumulative distribution function: In-database interface
 */
AnyType
students_t_cdf::run(AnyType &args) {
    return prob::cdf(
            students_t(args[1].getAs<double>()),
            args[0].getAs<double>()
        );
}

} // namespace prob

} // namespace modules

} // namespace madlib
