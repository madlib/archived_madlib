/* ----------------------------------------------------------------------- *//** 
 *
 * @file boost.cpp
 *
 * @brief Probability density and distribution functions imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/chi_squared.hpp>
#include <boost/math/distributions/normal.hpp>

namespace madlib {

namespace modules {

namespace prob {

// Workaround for Doxygen: Only ignore header file if processed stand-alone
#define MADLIB_PROB_BOOST_CPP
#include "boost.hpp"

/**
 * @brief Chi-squared cumulative distribution function: In-database interface
 */
AnyType
chi_squared_cdf::run(AnyType &args) {
    int64_t nu = args[0].getAs<int64_t>();
    double t = args[1].getAs<double>();
        
    /* We want to ensure nu > 0 */
    if (nu <= 0)
        throw std::domain_error("Chi Squared distribution undefined for "
            "degree of freedom <= 0");

    return chiSquaredCDF(nu, t);
}

double
chiSquaredCDF(int64_t nu, double t) {
    return boost::math::cdf( boost::math::chi_squared(nu), t );
}

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
