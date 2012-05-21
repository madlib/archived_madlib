/* ----------------------------------------------------------------------- *//** 
 *
 * @file boost.cpp
 *
 * @brief Probability density and distribution functions imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#include "boost.hpp"

namespace madlib {

namespace modules {

namespace prob {

#define DEFINE_PROBABILITY_FUNCTION_1(dist, what, argtype1) \
    AnyType \
    dist ## _ ## what::run(AnyType &args) { \
        return prob::what( \
                dist( \
                    args[1].getAs< argtype1 >() \
                ), \
                args[0].getAs<double>() \
            ); \
    }

#define DEFINE_PROBABILITY_FUNCTION_2(dist, what, argtype1, argtype2) \
    AnyType \
    dist ## _ ## what::run(AnyType &args) { \
        return prob::what( \
                dist( \
                    args[1].getAs< argtype1 >(), \
                    args[2].getAs< argtype2 >() \
                ), \
                args[0].getAs<double>() \
            ); \
    }

#define DEFINE_CONTINUOUS_PROB_DISTR_1(dist, argtype1) \
    DEFINE_PROBABILITY_FUNCTION_1(dist, cdf, argtype1) \
    DEFINE_PROBABILITY_FUNCTION_1(dist, pdf, argtype1) \
    DEFINE_PROBABILITY_FUNCTION_1(dist, quantile, argtype1)

#define DEFINE_CONTINUOUS_PROB_DISTR_2(dist, argtype1, argtype2) \
    DEFINE_PROBABILITY_FUNCTION_2(dist, cdf, argtype1, argtype2) \
    DEFINE_PROBABILITY_FUNCTION_2(dist, pdf, argtype1, argtype2) \
    DEFINE_PROBABILITY_FUNCTION_2(dist, quantile, argtype1, argtype2)

DEFINE_CONTINUOUS_PROB_DISTR_1(chi_squared, double)
DEFINE_CONTINUOUS_PROB_DISTR_1(exponential, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(beta, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(fisher_f, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(gamma, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(normal, double, double)

} // namespace prob

} // namespace modules

} // namespace regress
