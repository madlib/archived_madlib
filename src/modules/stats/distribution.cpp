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

AnyType
vectorized_distribution_transition::run(AnyType &args) {
    if (args[1].isNull() || args[2].isNull()) { return Null(); }

    // dimension information
    MappedIntegerVector levels = args[2].getAs<MappedIntegerVector>();
    // tuple
    MappedIntegerVector indices = args[1].getAs<MappedIntegerVector>();
    if (indices.size() != levels.size()) {
        std::stringstream ss;
        ss << "size mismatch between indices levels: "
            "indices.sizes=" << indices.size()
            << ", levels.size()=" << levels.size() << std::endl;
        throw std::runtime_error(ss.str());
    }
    // state
    MutableNativeMatrix distributions;
    if (args[0].isNull()) {
        // allocate the state for the first row
        if (levels.minCoeff() <= 0) {
            throw std::runtime_error("unexpected non-positive level");
        }
        // because Eigen is column-first and Postgres is row-first,
        // this matrix is levels.maxCoeff() x levels.size() when operated
        // using Eigen functions
        distributions.rebind(
                this->allocateArray<double>(levels.size(), levels.maxCoeff()));
    } else {
        // avoid distribution copying if initialized
        distributions.rebind(args[0].getAs<MutableArrayHandle<double> >());
    }

    for (Index i = 0; i < indices.size(); i ++) {
        int index = indices(i);
        if (index < 0 || index >= levels(i)) {
            std::stringstream ss;
            ss << "index out-of-bound: index=" << indices(i)
                << ", level=" << levels(i) << std::endl;
            throw std::runtime_error(ss.str());
        }
        distributions(index, i) ++;
    }

    return distributions;
}
// ------------------------------------------------------------

AnyType
vectorized_distribution_final::run(AnyType &args) {
    MutableNativeMatrix state = args[0].getAs<MutableNativeMatrix>();
    state /= state.sum();
    return state;
}

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
// ------------------------------------------------------------

AnyType
discrete_distribution_final::run(AnyType &args) {
    MutableNativeColumnVector state =
        args[0].getAs<MutableNativeColumnVector>();
    state /= state.sum();
    return state;
}


} // namespace recursive_partitioning
} // namespace modules
} // namespace madlib
