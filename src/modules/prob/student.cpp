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
 * @brief Student's t cumulative distribution function: In-database interface
 */
AnyType
students_t_cdf::run(AnyType &args) {
    return prob::cdf(
            students_t(args[1].getAs<double>()),
            args[0].getAs<double>()
        );
}

/**
 * @brief Student's t probability density function: In-database interface
 */
AnyType
students_t_pdf::run(AnyType &args) {
    return prob::pdf(
            students_t(args[1].getAs<double>()),
            args[0].getAs<double>()
        );
}

/**
 * @brief Student's t quantile function: In-database interface
 */
AnyType
students_t_quantile::run(AnyType &args) {
    return prob::quantile(
            students_t(args[1].getAs<double>()),
            args[0].getAs<double>()
        );
}

} // namespace prob

} // namespace modules

} // namespace madlib
