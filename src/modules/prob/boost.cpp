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
#include <boost/math/distributions/beta.hpp>
#include <cmath>

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
 * @brief Chi-squared distribution probability density function
 */
AnyType 
chi_squared_pdf::run(AnyType &args) {
    double t = args[0].getAs<double>();
    double nu = args[1].getAs<double>();
        
    /* We want to ensure nu > 0 */
    if (nu <= 0)
        throw std::domain_error("Chi Squared distribution undefined for "
            "degree of freedom < 0");

    return chiSquaredPDF(t, nu);
}

double 
chiSquaredPDF(double t, double nu) {
    if (nu <= 0 || std::isnan(t) || std::isnan(nu))
        return std::numeric_limits<double>::quiet_NaN();
    else if (t == std::numeric_limits<double>::infinity())
        return 0;
    else if (t < 0)
        return 0;
    
    if (0 == t) {
        if (nu > 2) {
            return 0;
        }
        else if (nu < 2) {
            return std::numeric_limits<double>::infinity();
        }
        return 0.5;
    }

    return boost::math::pdf( boost::math::chi_squared(nu), t );
}

/**
 * @brief Chi-squared distribution probability density function
 */
AnyType 
chi_squared_quantile::run(AnyType &args) {
    double t = args[0].getAs<double>();
    double nu = args[1].getAs<double>();
        
    /* We want to ensure nu > 0 */
    if (nu <= 0) {
        throw std::domain_error("Chi Squared distribution undefined for "
            "degree of freedom <= 0");
    }
    else if (t < 0) {
        throw std::domain_error("Chi Squared distribution undefined for "
            "cumulative < 0");
    }
    else if (t > 1) {
        throw std::domain_error("Chi Squared distribution undefined for "
            "cumulative > 1");
    }

    return chiSquaredQuantile(t, nu);
}

double 
chiSquaredQuantile(double t, double nu) {
    if (nu <= 0 || std::isnan(t) || std::isnan(nu) || t < 0 || t > 1) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    else if (t == 0) {
        return 0;
    }
    else if (t == 1) {
        return std::numeric_limits<double>::infinity();
    }

    return boost::math::quantile( boost::math::chi_squared(nu), t );
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
 * @brief Fisher F probability density function: In-database interface
 */
AnyType
fisher_f_pdf::run(AnyType &args) {
    double t = args[0].getAs<double>();
    double df1 = args[1].getAs<double>();
    double df2 = args[2].getAs<double>();
        
    if (df1 <= 0 || df2 <= 0) {
        throw std::domain_error("Fisher F distribution undefined for "
            "degrees of freedom <= 0");
    }

    return fisherF_PDF(t, df1, df2);
}

double fisherF_PDF(double t, double df1, double df2) {
    if (df1 <= 0 || df2 <= 0 ||
        std::isnan(t) || std::isnan(df1) || std::isnan(df2)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    else if (t == std::numeric_limits<double>::infinity()) {
        return 0;
    }
    else if (t < 0) {
        return 0;
    }
    else if (0 == t) {
        return df1 > 2 ? 0 : (df1 == 2 ? 1 : std::numeric_limits<double>::infinity());
    }
    
    return boost::math::pdf( boost::math::fisher_f(df1, df2), t );
}

/**
 * @brief Fisher F quantile function: In-database interface
 */
AnyType
fisher_f_quantile::run(AnyType &args) {
    double t = args[0].getAs<double>();
    double df1 = args[1].getAs<double>();
    double df2 = args[2].getAs<double>();
        
    if (df1 <= 0 || df2 <= 0) {
        throw std::domain_error("Fisher F distribution undefined for "
            "degrees of freedom <= 0");
    }
    else if (t > 1 || t < 0) {
        throw std::domain_error("Fisher F distribution undefined for "
            "CDF out of range [0, 1]");
    }
    
    return fisherF_Quantile(t,df1,df2);
}

double
fisherF_Quantile(double t, double df1, double df2)
{
    if (t < 0 || t > 1 || df1 <= 0 || df2 <= 0 ||
        std::isnan(t) || std::isnan(df1) || std::isnan(df2)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    else if (t == 0) {
        return 0;
    }
    else if (t == 1) {
        return std::numeric_limits<double>::infinity();
    }
    
    return boost::math::quantile( boost::math::fisher_f(df1, df2), t );
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
    else if (std::isinf(t)) {
        return 0;
    }
    
    return boost::math::pdf( boost::math::normal(mu, sigma), t );
}

AnyType
normal_quantile::run(AnyType &args) {
    double t = args[0].getAs<double>();
    double mu = args[1].getAs<double>();
    double sigma = args[2].getAs<double>();

    if (sigma < 0) {
        throw std::domain_error("Normal distribtion distribution undefined for "
            "standard deviation < 0");
    }
    else if (t > 1) {
        throw std::domain_error("Normal distribution distribution undefined for "
             "cumulative > 1");
    }
    else if (t < 0) {
        throw std::domain_error("Normal distribution distribution undefined for "
             "cumulative < 0");
    }
    return normalQuantile(t,mu,sigma);
}

double 
normalQuantile(double t, double mu, double sigma) {
    if (sigma < 0 || std::isnan(t) || std::isnan(mu) || std::isnan(sigma) || t > 1 || t< 0) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    else if (0 == t) {
        return -std::numeric_limits<double>::infinity();
    }
    else if (1 == t) {
        return std::numeric_limits<double>::infinity();
    }
    
    return boost::math::quantile(boost::math::normal(mu,sigma), t);
}


/**
 * @brief Beta cumulate distribution function: In-database interface
 */
AnyType
beta_cdf::run(AnyType &args) {
    double t = args[0].getAs<double>();
    double alpha = args[1].getAs<double>();
    double beta = args[2].getAs<double>();
        
    if (alpha <= 0 || beta <= 0) {
        throw std::domain_error("Beta distribution undefined for "
            "shape parameters <= 0");
    }

    return beta_CDF(t, alpha, beta);
}

double
beta_CDF(double t, double alpha, double beta) {
    if (alpha <= 0 || beta <= 0 ||
        std::isnan(t) || std::isnan(alpha) || std::isnan(beta)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    else if (t < 0 ) {
        return 0;
    }
    else if ( t > 1 ) {
        return 1;
    }
    
    return boost::math::cdf(boost::math::beta_distribution<double>(alpha, beta), t );
}

/**
 * @brief Beta probability density function: In-database interface
 */
AnyType
beta_pdf::run(AnyType &args) {
    double t = args[0].getAs<double>();
    double alpha = args[1].getAs<double>();
    double beta = args[2].getAs<double>();
        
    if (alpha <= 0 || beta <= 0) {
        throw std::domain_error("Beta distribution undefined for "
            "shape parameters <= 0");
    }

    return beta_PDF(t, alpha, beta);
}

double beta_PDF(double t, double alpha, double beta) {
    if (alpha <= 0 || beta <= 0 ||
        std::isnan(t) || std::isnan(alpha) || std::isnan(beta)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    else if (t < 0 || t > 1) {
        return 0;
    }
    
    return boost::math::pdf( boost::math::beta_distribution<double>(alpha, beta), t );
}

/**
 * @brief Beta quantile function: In-database interface
 */
AnyType
beta_quantile::run(AnyType &args) {
    double t = args[0].getAs<double>();
    double alpha = args[1].getAs<double>();
    double beta = args[2].getAs<double>();
        
    if (alpha <= 0 || beta <= 0) {
        throw std::domain_error("Beta distribution undefined for "
            "shape parameters <= 0");
    }
    else if (t > 1 || t < 0) {
        throw std::domain_error("Beta distribution undefined for "
            "CDF out of range [0, 1]");
    }
    
    return beta_Quantile(t,alpha,beta);
}

double
beta_Quantile(double t, double alpha, double beta)
{
    if (t < 0 || t > 1 || alpha <= 0 || beta <= 0 ||
        std::isnan(t) || std::isnan(alpha) || std::isnan(beta)) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    else if (t == 0) {
        return 0;
    }
    else if (t == 1) {
        return 1;
    }
    
    return boost::math::quantile( boost::math::beta_distribution<double>(alpha, beta), t );
}
} // namespace prob

} // namespace modules

} // namespace regress
