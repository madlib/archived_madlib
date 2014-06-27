/* ----------------------------------------------------------------------- *//**
 *
 * @file link.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_GLM_LINK_HPP
#define MADLIB_MODULES_GLM_LINK_HPP

#include <cmath>

namespace madlib {

namespace modules {

namespace glm {

using namespace std;

class Identity {
public:
    static double init(const double &y) { return y; }
    static double link_func(const double &mu) { return mu; }
    static double mean_func(const double &ita) { return ita; }
    static double mean_derivative(const double &) { return 1.; }
};

class Log {
public:
    static double init(const double &y) { return std::max(y, 0.1); }
    static double link_func(const double &mu) { return log(mu); }
    static double mean_func(const double &ita) { return exp(ita); }
    static double mean_derivative(const double &ita) { return exp(ita); }
};

class Sqrt {
public:
    static double init(const double &y) { return std::max(y, 0.); }
    static double link_func(const double &mu) { return sqrt(mu); }
    static double mean_func(const double &ita) { return ita * ita; }
    static double mean_derivative(const double &ita) { return 2 * ita; }
};

class Inverse
{
public:
    static double init(const double &y) { return y == 0 ? 0.1 : y; }
    static double link_func(const double &mu) { return 1./mu; }
    static double mean_func(const double &ita) { return 1./ita; }
    static double mean_derivative(const double &ita) { return -1./(ita*ita); }
};

class Sqr_inverse
{
public:
    static double init(const double &y) { return y == 0 ? 0.1 : y; }
    static double link_func(const double &mu) { return 1./mu/mu; }
    static double mean_func(const double &ita) { return 1./sqrt(ita); }
    static double mean_derivative(const double &ita) { return -1./2/sqrt(ita*ita*ita); }
};

} // namespace glm

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_GLM_LINK_HPP)
