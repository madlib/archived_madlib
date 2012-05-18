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
#include <boost/math/distributions/fisher_f.hpp>
#include <boost/math/distributions/normal.hpp>

#include "boost.hpp"

namespace madlib {

namespace modules {

namespace prob {

/**
 * @brief Chi-squared cumulative distribution function: In-database interface
 */
AnyType
chi_squared_cdf::run(AnyType &args) {
    double t = args[0].getAs<double>();
    double nu = args[1].getAs<double>();
        
    /* We want to ensure nu > 0 */
    if (nu <= 0)
        throw std::domain_error("Chi Squared distribution undefined for "
            "degree of freedom <= 0");

    return chiSquaredCDF(t, nu);
}

double
chiSquaredCDF(double t, double nu) {
    if (nu <= 0 || std::isnan(t) || std::isnan(nu))
        return std::numeric_limits<double>::quiet_NaN();
    else if (t == std::numeric_limits<double>::infinity())
        return 1;
    else if (t < 0)
        return 0;

    return boost::math::cdf( boost::math::chi_squared(nu), t );
}


/**
 * @brief Fisher F cumulate distribution function: In-database interface
 */
AnyType
fisher_f_cdf::run(AnyType &args) {
    double t = args[0].getAs<double>();
    double df1 = args[1].getAs<double>();
    double df2 = args[2].getAs<double>();
        
    if (df1 <= 0 || df2 <= 0)
        throw std::domain_error("Fisher F distribution undefined for "
            "degrees of freedom <= 0");

    return fisherF_CDF(t, df1, df2);
}

double
fisherF_CDF(double t, double df1, double df2) {
    if (df1 <= 0 || df2 <= 0 ||
        std::isnan(t) || std::isnan(df1) || std::isnan(df2))
        return std::numeric_limits<double>::quiet_NaN();
    else if (t == std::numeric_limits<double>::infinity())
        return 1;
    else if (t < 0)
        return 0;
    
    return boost::math::cdf( boost::math::fisher_f(df1, df2), t );
}


/**
 * @brief Normal cumulative distribution function: In-database interface
 */
AnyType
normal_cdf::run(AnyType &args) {
    double t = args[0].getAs<double>();
    double mu = args[1].getAs<double>();
    double sigma = args[2].getAs<double>();

    if (sigma < 0)
        throw std::domain_error("Normal distribtion distribution undefined for "
            "standard deviation < 0");
    
    return normalCDF(t, mu, sigma);
}

double
normalCDF(double t, double mu, double sigma) {
    if (sigma < 0 || std::isnan(t) || std::isnan(mu) || std::isnan(sigma))
        return std::numeric_limits<double>::quiet_NaN();
    else if (t == std::numeric_limits<double>::infinity())
        return 1;
    else if (t == -std::numeric_limits<double>::infinity())
        return 0;
    
    return boost::math::cdf( boost::math::normal(mu, sigma), t );
}

} // namespace prob

} // namespace modules

} // namespace regress
