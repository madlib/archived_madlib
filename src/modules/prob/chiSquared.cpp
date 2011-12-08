/* ----------------------------------------------------------------------- *//** 
 *
 * @file chiSquared.cpp
 *
 * @brief Evaluate the chi-squared distribution function using the boost library.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/chi_squared.hpp>

namespace madlib {

namespace modules {

namespace prob {

#include "chiSquared.hpp"

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

    return chiSquared_cdf(nu, t);
}

inline
double
chiSquared_cdf(int64_t nu, double t) {
    return boost::math::cdf( boost::math::chi_squared(nu), t );
}

} // namespace prob

} // namespace modules

} // namespace regress
