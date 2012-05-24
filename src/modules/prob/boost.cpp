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

#define DEFINE_PROBABILITY_FUNCTION_1(dist, what, boost_what, rvtype, argtype1) \
    AnyType \
    dist ## _ ## what::run(AnyType &args) { \
        return prob::boost_what( \
                dist( \
                    args[1].getAs< argtype1 >() \
                ), \
                static_cast<double>(args[0].getAs< rvtype >()) \
            ); \
    }

#define DEFINE_PROBABILITY_FUNCTION_2(dist, what, boost_what, rvtype, \
    argtype1, argtype2) \
    AnyType \
    dist ## _ ## what::run(AnyType &args) { \
        return prob::boost_what( \
                dist( \
                    args[1].getAs< argtype1 >(), \
                    args[2].getAs< argtype2 >() \
                ), \
                static_cast<double>(args[0].getAs< rvtype >()) \
            ); \
    }

#define DEFINE_PROBABILITY_FUNCTION_3(dist, what, boost_what, rvtype, \
    argtype1, argtype2, argtype3) \
    AnyType \
    dist ## _ ## what::run(AnyType &args) { \
        return prob::boost_what( \
                dist( \
                    args[1].getAs< argtype1 >(), \
                    args[2].getAs< argtype1 >(), \
                    args[3].getAs< argtype2 >() \
                ), \
                static_cast<double>(args[0].getAs< rvtype >()) \
            ); \
    }

#define DEFINE_PROBABILITY_DISTR_1(dist, pdf_or_pmf, rvtype, argtype1) \
    DEFINE_PROBABILITY_FUNCTION_1(dist, cdf, cdf, double, argtype1) \
    DEFINE_PROBABILITY_FUNCTION_1(dist, pdf_or_pmf, pdf, rvtype, argtype1) \
    DEFINE_PROBABILITY_FUNCTION_1(dist, quantile, quantile, double, argtype1)

#define DEFINE_PROBABILITY_DISTR_2(dist, pdf_or_pmf, rvtype, argtype1, \
    argtype2) \
    DEFINE_PROBABILITY_FUNCTION_2(dist, cdf, cdf, double, \
        argtype1, argtype2) \
    DEFINE_PROBABILITY_FUNCTION_2(dist, pdf_or_pmf, pdf, rvtype, \
        argtype1, argtype2) \
    DEFINE_PROBABILITY_FUNCTION_2(dist, quantile, quantile, double, \
        argtype1, argtype2)

#define DEFINE_PROBABILITY_DISTR_3(dist, pdf_or_pmf, rvtype, \
    argtype1, argtype2, argtype3) \
    DEFINE_PROBABILITY_FUNCTION_3(dist, cdf, cdf, double, \
        argtype1, argtype2, argtype3) \
    DEFINE_PROBABILITY_FUNCTION_3(dist, pdf_or_pmf, pdf, rvtype, \
        argtype1, argtype2, argtype3) \
    DEFINE_PROBABILITY_FUNCTION_3(dist, quantile, quantile, double, \
        argtype1, argtype2, argtype3)

#define DEFINE_CONTINUOUS_PROB_DISTR_1(dist, argtype1) \
    DEFINE_PROBABILITY_DISTR_1(dist, pdf, double, argtype1)

#define DEFINE_CONTINUOUS_PROB_DISTR_2(dist, argtype1, argtype2) \
    DEFINE_PROBABILITY_DISTR_2(dist, pdf, double, argtype1, argtype2)

#define DEFINE_CONTINUOUS_PROB_DISTR_3(dist, argtype1, argtype2, argtype3) \
    DEFINE_PROBABILITY_DISTR_3(dist, pdf, double, argtype1, argtype2, argtype3)

#define DEFINE_DISCRETE_PROB_DISTR_1(dist, rvtype, argtype1) \
    DEFINE_PROBABILITY_DISTR_1(dist, pmf, rvtype, argtype1)

#define DEFINE_DISCRETE_PROB_DISTR_2(dist, rvtype, argtype1, argtype2) \
    DEFINE_PROBABILITY_DISTR_2(dist, pmf, rvtype, argtype1, argtype2)

#define DEFINE_DISCRETE_PROB_DISTR_3(dist, rvtype, \
    argtype1, argtype2, argtype3) \
    DEFINE_PROBABILITY_DISTR_3(dist, pmf, rvtype, argtype1, argtype2, argtype3)

DEFINE_CONTINUOUS_PROB_DISTR_2(beta, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(cauchy, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_1(chi_squared, double)
DEFINE_CONTINUOUS_PROB_DISTR_1(exponential, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(extreme_value, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(fisher_f, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(gamma, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(inverse_chi_squared, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(inverse_gamma, double, double)
// FIXME: Pending Boost bug 6934, we currently do not support the
// inverse Gaussian distribution. https://svn.boost.org/trac/boost/ticket/6934
// DEFINE_CONTINUOUS_PROB_DISTR_2(inverse_gaussian, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(laplace, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(logistic, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(lognormal, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_3(non_central_beta, double, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(non_central_chi_squared, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_3(non_central_f, double, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(non_central_t, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(normal, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(pareto, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_1(rayleigh, double)
// For Student's t distribution, see student.cpp
DEFINE_CONTINUOUS_PROB_DISTR_3(triangular, double, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(uniform, double, double)
DEFINE_CONTINUOUS_PROB_DISTR_2(weibull, double, double)

DEFINE_DISCRETE_PROB_DISTR_1(bernoulli, uint32_t, double)
DEFINE_DISCRETE_PROB_DISTR_2(binomial, uint64_t, uint64_t, double)
DEFINE_DISCRETE_PROB_DISTR_1(geometric, uint64_t, double)
DEFINE_DISCRETE_PROB_DISTR_3(hypergeometric, uint64_t, uint64_t, uint64_t,
    uint64_t)
DEFINE_DISCRETE_PROB_DISTR_2(negative_binomial, uint64_t, double, double)
DEFINE_DISCRETE_PROB_DISTR_1(poisson, uint64_t, double)

} // namespace prob

} // namespace modules

} // namespace regress
