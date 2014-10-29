/* ----------------------------------------------------------------------- *//**
 *
 * @file random_process.cpp
 *
 * @brief Generate a random number from a random process
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <boost/random/poisson_distribution.hpp>
#include <boost/random/variate_generator.hpp>

#include "random_process.hpp"

using boost::poisson_distribution;
using boost::variate_generator;

namespace madlib {

namespace modules {

namespace sample {

/**
 * @brief Perform the merging of two transition states
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

} // namespace sample

} // namespace modules

} // namespace madlib
