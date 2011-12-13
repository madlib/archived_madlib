/* ----------------------------------------------------------------------- *//** 
 *
 * @file normal.cpp
 *
 * @brief Evaluate the standard normal distribution function using the boost
 *     library.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/normal.hpp>

namespace madlib {

namespace modules {

namespace prob {

#include "normal.hpp"

/**
 * @brief Normal cumulative distribution function: In-database interface
 */
AnyType
normal_cdf::run(AnyType &args) {
    return normalCDF(args[0].getAs<double>());
}

double
normalCDF(double t) {
    return boost::math::cdf( boost::math::normal(), t );
}

} // namespace prob

} // namespace modules

} // namespace regress
