/* ----------------------------------------------------------------------- *//**
 *
 * @file weighted_sample.cpp
 *
 * @brief Generate a single weighted random sample
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>

#include <boost/tr1/random.hpp>

#include "weighted_sample.hpp"

// Import TR1 names (currently used from boost). This can go away once we make
// the switch to C++11.
namespace std {
    using tr1::bernoulli_distribution;
}

namespace madlib {

namespace modules {

namespace sample {

/**
 * @brief Transition state for weighted sample
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 2, and all elemenets are 0.
 */
template <class Handle>
class WeightedSampleTransitionState {
public:
    WeightedSampleTransitionState(const AnyType &inArray)
      : mStorage(inArray.getAs<Handle>()),
        sample_id(&mStorage[1]),
        weight_sum(&mStorage[0]) { }

    inline operator AnyType() const {
        return mStorage;
    }

private:
    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToInt64 sample_id;
    typename HandleTraits<Handle>::ReferenceToDouble weight_sum;
};

/**
 * @brief Perform the weighted-sample transition step
 */
AnyType
weighted_sample_transition::run(AnyType& args) {
    WeightedSampleTransitionState<MutableArrayHandle<double> > state = args[0];
    uint64_t identifier = args[1].getAs<int64_t>();
    double weight = args[2].getAs<double>();

    // Instead of throwing an error, we will just ignore rows with a negative
    // weight
    if (weight > 0.) {
        state.weight_sum += weight;
        std::bernoulli_distribution success(weight / state.weight_sum);
        // Note that a NativeRandomNumberGenerator object is stateless, so it
        // is not a problem to instantiate an object for each RN generation...
        NativeRandomNumberGenerator generator;
        if (success(generator))
            state.sample_id = identifier;
    }

    return state;
}

/**
 * @brief Perform the merging of two transition states
 */
AnyType
weighted_sample_merge::run(AnyType &args) {
    WeightedSampleTransitionState<MutableArrayHandle<double> > stateLeft
        = args[0];
    WeightedSampleTransitionState<ArrayHandle<double> > stateRight = args[1];

    // FIXME: Once we have more modular states (independent of transition/merge
    // function), implement using the logic from the transition function
    stateLeft.weight_sum += stateRight.weight_sum;
    std::bernoulli_distribution success(
        stateRight.weight_sum / stateLeft.weight_sum);
    // Note that a NativeRandomNumberGenerator object is stateless, so it
    // is not a problem to instantiate an object for each RN generation...
    NativeRandomNumberGenerator generator;
    if (success(generator))
        stateLeft.sample_id = stateRight.sample_id;

    return stateLeft;
}

/**
 * @brief Perform the weighted-sample final step
 */
AnyType
weighted_sample_final::run(AnyType &args) {
    WeightedSampleTransitionState<ArrayHandle<double> > state = args[0];

    return static_cast<int64_t>(state.sample_id);
}

} // namespace stats

} // namespace modules

} // namespace madlib
