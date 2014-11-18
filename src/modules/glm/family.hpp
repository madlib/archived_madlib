/* ----------------------------------------------------------------------- *//**
 *
 * @file family.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_GLM_FAMILY_HPP
#define MADLIB_MODULES_GLM_FAMILY_HPP

#include <cmath>
#include <limits>

using namespace madlib::dbal::eigen_integration;

namespace madlib {

namespace modules {

namespace glm {

class Gaussian {
public:
    static double variance(const double &) { return 1.; }
    static double loglik(const double &y, const double &mu, const double &psi) {
        double theta = mu;
        double a = psi;
        double b = theta * theta / 2.;
        double c = - y * y / (a * 2.);
        c -= std::log(std::sqrt(2 * M_PI * a));

        return (y * theta - b) / a + c;
    }
    static std::string out_of_range_err_msg() {
        throw std::runtime_error("no out-of-range error expected (gaussian)");
    }
    static bool in_range(const double &) { return true; }
};

class Poisson {
public:
    static double variance(const double &mu) { return mu; }
    static double loglik(const double &y, const double &mu, const double &) {
        if (mu <= 0) return - std::numeric_limits<double>::infinity();
        double theta = std::log(mu);
        double a = 1.;
        double b = mu;
        double c = 0.;
        unsigned i = 0;
        for (i = 2; i <= y; i ++) { c -= std::log(i); }

        return (y * theta - b) / a + c;
    }
    static std::string out_of_range_err_msg() {
        return "non-negative integers expected (poisson)"; }
    static bool in_range(const double &y) {
        double intpart;
        return (modf(y, &intpart) == 0.0 && (y >= 0.0));
    }
};

class Gamma {
public:
    static double variance(const double &mu) { return mu*mu; }
    static double loglik(const double &y, const double &mu, const double &psi) {
        double theta = -1./mu;
        double a = psi;
        double b = -std::log(-theta);
        double c = 1./psi*std::log(y/psi) - std::log(y) - lgamma(1./psi);

        return (y * theta - b) / a + c;
    }
    static std::string out_of_range_err_msg() {
        return "non-negative expected (gamma).";
    }
    static bool in_range(const double &y) { return (y >= 0.0); }
};

class InverseGaussian {
public:
    static double variance(const double &mu) { return mu*mu*mu; }
    static double loglik(const double &y, const double &mu, const double &psi) {
        double theta = 1./2./mu/mu;
        double a = -psi;
        double b = 1./mu;
        double c = -1./(2*y*psi) - 1./2.*std::log(2.*M_PI*y*y*y*psi);

        return (y * theta - b) / a + c;
    }
    static std::string out_of_range_err_msg() {
        return "non-negative expected (inverse_gaussian).";
    }
    static bool in_range(const double &y) { return (y >= 0.0); }
};

class Binomial {
public:
    static double variance(const double &mu) { return mu * (1 - mu); }
    static double loglik(const double &y, const double &mu, const double &) {
        if (mu == 0 || mu == 1) return 0;
        double theta = log(mu / (1 - mu));
        double a = 1;
        double b = 0;
        double c = log(1 - mu);

        return (y * theta - b) / a + c;
    }
    static std::string out_of_range_err_msg() {
        return "boolean expected (binomial)";
    }
    static bool in_range(const double &y) {
        double intpart;
        return (modf(y, &intpart)==0.0 && (y == 0.0 || y == 1.0));
    }
};

class Multinomial {
public:
    static void variance(const ColumnVector &mu, Matrix &mVar) {
       for (int i = 0; i < mu.size(); i ++) {
         for (int j = 0; j < mu.size(); j ++) {
           if (i == j) { mVar(i,j) = mu(i) * (1-mu(i)); }
           else { mVar(i,j) = -mu(i)*mu(j); }
         }
       }
    }
    static double loglik(const ColumnVector &y, const ColumnVector &mu,
            const double &) {

        double result = 0;
        for (int i = 0; i < y.size(); i ++) {
            result += y(i) * std::log(mu(i));
        }
        result += (1-y.sum()) * std::log(1-mu.sum());

        return result;
    }
};

} // namespace glm

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_GLM_FAMILY_HPP)

