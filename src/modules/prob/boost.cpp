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

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
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
    return std::isnan(t)
        ? std::numeric_limits<double>::quiet_NaN()
        : boost::math::cdf( boost::math::normal(), t );
}


/**
 * @brief Normal probability density function: In-database interface
 */
AnyType
normal_pdf::run(AnyType &args) {
    double t = args[0].getAs<double>();
    double mu = args[1].getAs<double>();
    double sigma = args[2].getAs<double>();

    if (sigma < 0) {
        throw std::domain_error("Normal distribtion distribution undefined for "
            "standard deviation < 0");
    }
    
    return normalPDF(t, mu, sigma);
}

double
normalPDF(double t, double mu, double sigma) {
    if (sigma < 0 || std::isnan(t) || std::isnan(mu) || std::isnan(sigma)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    else if (t == std::numeric_limits<double>::infinity()) {
        return 1;
    }
    else if (t == -std::numeric_limits<double>::infinity()) {
        return 0;
    }
    
    return boost::math::pdf( boost::math::normal(mu, sigma), t );
}

} // namespace prob

} // namespace modules

} // namespace regress
