/* ----------------------------------------------------------------------- *//**
 *
 * @file random_process.cpp
 *
 * @brief Generate a random number from a random process
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <boost/random/poisson_distribution.hpp>
#include <boost/random/gamma_distribution.hpp>
#include <boost/random/variate_generator.hpp>

#include "random_process.hpp"

using boost::poisson_distribution;
using boost::gamma_distribution;
using boost::variate_generator;

namespace madlib {

namespace modules {

namespace sample {

/**
 * @brief Poisson distributed random variables given mean
 */
AnyType
poisson_random::run(AnyType &args) {
    double mean = args[0].getAs<double>();
    poisson_distribution<int> pdist(mean);
    NativeRandomNumberGenerator generator;
    variate_generator<NativeRandomNumberGenerator, poisson_distribution<int> >
            rvt(generator, pdist);

    return rvt();
}

/**
 * @brief Gamma distributed random variables given alpha
 */
AnyType
gamma_random::run(AnyType &args) {
    double alpha = args[0].getAs<double>();
    gamma_distribution<> gdist(alpha);
    NativeRandomNumberGenerator generator;
    variate_generator<NativeRandomNumberGenerator, gamma_distribution<> >
            rvt(generator, gdist);

    return rvt();
}

} // namespace sample

} // namespace modules

} // namespace madlib
