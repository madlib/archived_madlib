/* ----------------------------------------------------------------------- *//**
 *
 * @file avgerage.cpp
 *
 * @brief Compute the average of vectors
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>

#include "average.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

namespace linalg {

/**
 * @brief Transition state for computing average of vectors
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 3, and all elemenets are 0. Handle::operator[] will
 * perform bounds checking.
 */
template <class Handle>
class AvgVectorState {
    template <class OtherHandle>
    friend class AvgVectorState;

public:
    AvgVectorState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint32_t>(mStorage[1]));
    }

    inline operator AnyType() const {
        return mStorage;
    }

    inline void initialize(const Allocator &inAllocator,
        uint32_t inNumDimensions) {

        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inNumDimensions));
        rebind(inNumDimensions);
        numDimensions = inNumDimensions;
    }

    template <class OtherHandle>
    AvgVectorState &operator+=(
        const AvgVectorState<OtherHandle> &inOtherState) {

        numRows += inOtherState.numRows;
        sumOfVectors += inOtherState.sumOfVectors;
        return *this;
    }

private:
    static inline size_t arraySize(uint32_t inNumDimensions) {
        return static_cast<size_t>(2 + inNumDimensions);
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inNumDimensions The number of dimensions
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: numRows (number of rows already processed in this iteration)
     * - 1: numDimensions (dimension of space that points are from)
     * - 2: sumOfPoints (vector with \c numDimensions rows)
     */
    void rebind(uint32_t inNumDimensions) {
        numRows.rebind(&mStorage[0]);
        numDimensions.rebind(&mStorage[1]);
        sumOfVectors.rebind(&mStorage[2], inNumDimensions);
        madlib_assert(mStorage.size() >= arraySize(inNumDimensions),
            std::runtime_error("Out-of-bounds array access detected."));
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt32 numDimensions;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap
        sumOfVectors;
};


AnyType
avg_vector_transition::run(AnyType& args) {
    AvgVectorState<MutableArrayHandle<double> > state = args[0];
    if (args[1].isNull()) { return args[0]; }
    MappedColumnVector x;
    try {
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    if (state.numRows == 0)
        state.initialize(*this, static_cast<uint32_t>(x.size()));
    else if (x.size() != state.sumOfVectors.size()
        || state.numDimensions !=
            static_cast<uint32_t>(state.sumOfVectors.size()))
        throw std::invalid_argument("Invalid arguments: Dimensions of points "
            "not consistent.");

    ++state.numRows;
    state.sumOfVectors += x;
    return state;
}

AnyType
avg_vector_merge::run(AnyType& args) {
    AvgVectorState<MutableArrayHandle<double> > stateLeft = args[0];
    AvgVectorState<ArrayHandle<double> > stateRight = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.numRows == 0)
        return stateRight;
    else if (stateRight.numRows == 0)
        return stateLeft;

    if (stateLeft.numDimensions != stateRight.numDimensions) {
        throw std::invalid_argument("Invalid arguments: Dimensions of points "
            "not consistent.");
    }

    stateLeft += stateRight;
    return stateLeft;
}

AnyType
avg_vector_final::run(AnyType& args) {
    AvgVectorState<ArrayHandle<double> > state = args[0];

    MutableNativeColumnVector avgVector(allocateArray<double>(
        state.sumOfVectors.size()));
    avgVector = state.sumOfVectors / static_cast<double>(state.numRows);
    return avgVector;
}


AnyType
normalized_avg_vector_transition::run(AnyType& args) {
    AvgVectorState<MutableArrayHandle<double> > state = args[0];
    if (args[1].isNull()) { return args[0]; }
    MappedColumnVector x;
    try {
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    if (state.numRows == 0)
        state.initialize(*this, static_cast<uint32_t>(x.size()));
    else if (x.size() != state.sumOfVectors.size()
        || state.numDimensions !=
            static_cast<uint32_t>(state.sumOfVectors.size()))
        throw std::invalid_argument("Invalid arguments: Dimensions of points "
            "not consistent.");

    ++state.numRows;
    state.sumOfVectors += x.normalized();
    return state;
}

AnyType
normalized_avg_vector_final::run(AnyType& args) {
    AvgVectorState<ArrayHandle<double> > state = args[0];

    MutableNativeColumnVector avgVector(allocateArray<double>(
        state.sumOfVectors.size()));
    avgVector = (state.sumOfVectors /
        static_cast<double>(state.numRows)).normalized();
    return avgVector;
}


} // namespace linalg

} // namespace modules

} // namespace regress
