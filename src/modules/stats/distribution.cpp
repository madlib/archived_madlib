/* ------------------------------------------------------
 *
 * @file distribution.cpp
 *
 * @brief Aggregate functions for collecting distributions
 *
 */ /* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <sstream>

#include "distribution.hpp"

namespace madlib {

namespace modules {

namespace stats {

// Use Eigen
using namespace dbal;
using namespace dbal::eigen_integration;
// ------------------------------------------------------------
// ------------------------------------------------------------

AnyType
discrete_distribution_transition::run(AnyType &args) {
    MutableNativeColumnVector distribution;
    if (args[0].isNull()) {
        // allocate the state for the first row
        int level = args[3].getAs<int>();
        if (level <= 0) {
            throw std::runtime_error("unexpected non-positive level");
        }
        distribution.rebind(this->allocateArray<double>(level));
    } else {
        // avoid distribution copying if initialized
        distribution.rebind(args[0].getAs<MutableArrayHandle<double> >());
    }

    int index = args[1].getAs<int>();
    double weight = args[2].getAs<double>();
    if (index < 0 || index >= distribution.size()) {
        std::stringstream ss;
        ss << "index out-of-bound: index=" << index
            << ", level=" << distribution.size() << std::endl;
        throw std::runtime_error(ss.str());
    }
    distribution[index] += weight;

    return distribution;
}

AnyType
discrete_distribution_merge::run(AnyType &args) {
    if (args[0].isNull()) { return args[1]; }
    if (args[1].isNull()) { return args[0]; }

    MutableNativeColumnVector state0 =
            args[0].getAs<MutableNativeColumnVector>();
    MappedColumnVector state1 = args[1].getAs<MappedColumnVector>();

    state0 += state1;
    return state0;
}

} // namespace recursive_partitioning
} // namespace modules
} // namespace madlib
