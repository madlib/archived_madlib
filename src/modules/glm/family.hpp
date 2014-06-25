/* ----------------------------------------------------------------------- *//**
 *
 * @file family.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_GLM_FAMILY_HPP
#define MADLIB_MODULES_GLM_FAMILY_HPP

#include <cmath>
#include <limits>

namespace madlib {

namespace modules {

namespace glm {

class Gaussian {
public:
    static double variance(const double &) { return 1.; }
    static double loglik(const double &y, const double &mu,
            const double &psi) {

        double theta = mu;
        double a = psi;
        double b = theta * theta / 2;
        double c = - y * y / (a * 2);
        c -= std::log(std::sqrt(2 * M_PI * a));

        return (y * theta - b) / a + c;
    }
};

class Poisson {
public:
    static double variance(const double &mu) { return mu; }
    static double loglik(const double &y, const double &mu,
            const double & /* psi */) {
        if (mu == 0) return - std::numeric_limits<double>::infinity();
        double theta = std::log(mu);
        double a = 1;
        double b = mu;
        double c = 0.;
        unsigned i = 0;
        for (i = 2; i <= y; i ++) { c -= std::log(i); }

        return (y * theta - b) / a + c;
    }
};

} // namespace glm

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_GLM_FAMILY_HPP)
