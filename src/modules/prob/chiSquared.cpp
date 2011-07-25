/* ----------------------------------------------------------------------- *//** 
 *
 * @file chiSquared.cpp
 *
 * @brief Evaluate the chi-squared distribution function using the boost library.
 *
 *//* ----------------------------------------------------------------------- */

#include <modules/prob/chiSquared.hpp>

#include <boost/math/distributions/chi_squared.hpp>


namespace madlib {

namespace modules {

namespace prob {

/**
 * @brief Chi-squared cumulative distribution function: In-database interface
 */
AnyType chi_squared_cdf(AbstractDBInterface &db, AnyType args) {
    AnyType::iterator arg(args);

    // Arguments from SQL call
    const int64_t nu = *arg++;
    const double t = *arg;
        
    /* We want to ensure nu > 0 */
    if (nu <= 0)
        throw std::domain_error("Chi Squared distribution undefined for "
            "degree of freedom <= 0");

    return boost::math::cdf( boost::math::chi_squared(nu), t );
}


} // namespace prob

} // namespace modules

} // namespace regress
