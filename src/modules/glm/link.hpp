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
    static double link_func(const double &mu) { return mu; }
    static double mean_func(const double &ita) { return ita; }
    static double mean_derivative(const double &) { return 1.; }
};

class Log {
public:
    static double link_func(const double &mu) { return log(mu); }
    static double mean_func(const double &ita) { return exp(ita); }
    static double mean_derivative(const double &ita) { return exp(ita); }
};

class Sqrt {
public:
    static double link_func(const double &mu) { return sqrt(mu); }
    static double mean_func(const double &ita) { return ita * ita; }
    static double mean_derivative(const double &ita) { return 2 * ita; }
};

} // namespace glm

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_GLM_LINK_HPP)
