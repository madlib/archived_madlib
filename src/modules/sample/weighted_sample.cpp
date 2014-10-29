/* ----------------------------------------------------------------------- *//**
 *
 * @file weighted_sample.cpp
 *
 * @brief Generate a single weighted random sample
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#include <boost/random/discrete_distribution.hpp>

#include "WeightedSample_proto.hpp"
#include "WeightedSample_impl.hpp"
#include "weighted_sample.hpp"

namespace madlib {

namespace modules {

namespace sample {

typedef WeightedSampleAccumulator<RootContainer, int64_t>
    WeightedSampleInt64State;
typedef WeightedSampleAccumulator<MutableRootContainer, int64_t>
    MutableWeightedSampleInt64State;

typedef WeightedSampleAccumulator<RootContainer, MappedColumnVector>
    WeightedSampleColVecState;
typedef WeightedSampleAccumulator<MutableRootContainer, MappedColumnVector>
    MutableWeightedSampleColVecState;


/**
 * @brief Perform the weighted-sample transition step
 */
AnyType
weighted_sample_transition_int64::run(AnyType& args) {
    MutableWeightedSampleInt64State state = args[0].getAs<MutableByteString>();
    int64_t x = args[1].getAs<int64_t>();
    double weight = args[2].getAs<double>();

    state << WeightedSampleInt64State::tuple_type(x, weight);
    return state.storage();
}

AnyType
weighted_sample_transition_vector::run(AnyType& args) {
    MutableWeightedSampleColVecState state = args[0].getAs<MutableByteString>();
    MappedColumnVector x = args[1].getAs<MappedColumnVector>();
    double weight = args[2].getAs<double>();

    state << WeightedSampleColVecState::tuple_type(x, weight);
    return state.storage();
}



/**
 * @brief Perform the merging of two transition states
 */
AnyType
weighted_sample_merge_int64::run(AnyType &args) {
    MutableWeightedSampleInt64State stateLeft
        = args[0].getAs<MutableByteString>();
    WeightedSampleInt64State stateRight = args[1].getAs<ByteString>();

    stateLeft << stateRight;
    return stateLeft.storage();
}

AnyType
weighted_sample_merge_vector::run(AnyType &args) {
    MutableWeightedSampleColVecState stateLeft
        = args[0].getAs<MutableByteString>();
    WeightedSampleColVecState stateRight = args[1].getAs<ByteString>();

    stateLeft << stateRight;
    return stateLeft.storage();
}


/**
 * @brief Perform the weighted-sample final step
 */
AnyType
weighted_sample_final_int64::run(AnyType &args) {
    WeightedSampleInt64State state = args[0].getAs<MutableByteString>();

    return static_cast<int64_t>(state.sample);
}

AnyType
weighted_sample_final_vector::run(AnyType &args) {
    WeightedSampleColVecState state = args[0].getAs<MutableByteString>();

    return state.sample;
}

/**
 * @brief In-memory weighted sample, returning index
 */
AnyType
index_weighted_sample::run(AnyType &args) {
    MappedColumnVector distribution;
    try {
        MappedColumnVector xx = args[0].getAs<MappedColumnVector>();
        distribution.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return Null();
    }

    boost::random::discrete_distribution<> dist(distribution.data(),
            distribution.data() + distribution.size());

    NativeRandomNumberGenerator gen;
    return dist(gen);
}

} // namespace sample

} // namespace modules

} // namespace madlib
