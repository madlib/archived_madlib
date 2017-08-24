
/**
   Functions that compute array average and standard deviation
*/

#include "dbconnector/dbconnector.hpp"
#include "modules/shared/HandleTraits.hpp"
#include "utils_regularization.hpp"

#include <float.h>

namespace madlib {
namespace modules {
namespace convex {

using namespace madlib::dbal::eigen_integration;

template <class Handle>
class ScalesState
{
    template <class OtherHandle> friend class ScalesState;

  public:
    ScalesState (const AnyType& inArray):
        mStorage(inArray.getAs<Handle>())
    {
        rebind();
    }

    inline operator AnyType () const
    {
        return mStorage;
    }

    inline void allocate (const Allocator& inAllocator,
                          uint32_t inDimension)
    {
        mStorage = inAllocator.allocateArray<double,
                                             dbal::AggregateContext,
                                             dbal::DoZero,
                                             dbal::ThrowBadAlloc>(
                                                 arraySize(inDimension));
        dimension.rebind(&mStorage[0]);
        dimension = inDimension;
        rebind();
    }

    template <class OtherHandle>
    ScalesState& operator= (const ScalesState<OtherHandle>& inOtherState)
    {
        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    static inline uint32_t arraySize (const uint32_t inDimension)
    {
        return 2 + 2 * inDimension;
    }

  protected:
    void rebind ()
    {
        dimension.rebind(&mStorage[0]);
        numRows.rebind(&mStorage[1]);
        mean.rebind(&mStorage[2], dimension);
        std.rebind(&mStorage[2 + dimension], dimension);
    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt32 dimension;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap mean;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap std;
};

// ------------------------------------------------------------------------
bool close_to_zero(double number) {
    if(fabs(number - 0.0) <= DBL_EPSILON*fabs(number)) {
        return true;
    }
    return false;
}

AnyType utils_var_scales_transition::run (AnyType& args)
{
    ScalesState<MutableArrayHandle<double> > state = args[0];
    MappedColumnVector x(NULL);
    try {
        new (&x) MappedColumnVector(args[1].getAs<MappedColumnVector>());
    } catch(const ArrayWithNullException &e) {
      //  warning("Input array contains NULL values, skipping it for mean and std computations");
        return state;
    }
    if (state.numRows == 0)
    {
        int dimension = args[2].getAs<int>();

        state.allocate(*this, dimension);
        state.dimension = dimension;
        state.numRows = 0;
        state.mean.setZero();
        state.std.setZero();
    }

    for (uint32_t i = 0; i < state.dimension; i++)
    {
        state.mean(i) += x(i);
        state.std(i) += x(i) * x(i);
    }

    state.numRows++;
    return state;
}

// ------------------------------------------------------------------------

AnyType utils_var_scales_merge::run (AnyType& args)
{
    ScalesState<MutableArrayHandle<double> > state1 = args[0];
    ScalesState<MutableArrayHandle<double> > state2 = args[1];

    if (state1.numRows == 0)
        return state2;
    else if (state2.numRows == 0)
        return state1;

    state1.mean += state2.mean;
    state1.std += state2.std;
    state1.numRows += state2.numRows;

    return state1;
}

// ------------------------------------------------------------------------

AnyType utils_var_scales_final::run (AnyType& args)
{
    ScalesState<MutableArrayHandle<double> > state = args[0];

    if (state.numRows == 0) return Null();

    state.mean /= static_cast<double>(state.numRows);
    state.std /= static_cast<double>(state.numRows);
    for (uint32_t i = 0; i < state.dimension; i++)
        state.std(i) = sqrt(state.std(i) - state.mean(i) * state.mean(i));

    return state;
}

// ------------------------------------------------------------------------
/*
This final function is used when we want to have a standard deviation
of 1.0 instead of 0.0 for any dimension. This is most likely to be used
by neural networks, since in other modules such as elastic_net, a std value
of 0.0 results in setting the corresponding coefficient value to 0.0 as well.
But in neural networks, we will end up using a std value of 1.0 instead.
*/
AnyType utils_var_scales_non_zero_std_final::run (AnyType& args)
{
    ScalesState<MutableArrayHandle<double> > state = args[0];

    if (state.numRows == 0) return Null();

    state.mean /= static_cast<double>(state.numRows);
    state.std /= static_cast<double>(state.numRows);
    for (uint32_t i = 0; i < state.dimension; i++) {
        if (state.numRows == 1) {
            state.mean(i) = 0.0;
            state.std(i) = 1.0;
        }
        else {
            state.std(i) = sqrt(state.std(i) - state.mean(i) * state.mean(i));
            if(close_to_zero(state.std(i)))
                state.std(i) = 1.0;
        }
    }

    return state;
}

// ------------------------------------------------------------------------

AnyType __utils_var_scales_result::run (AnyType& args)
{
    ScalesState<ArrayHandle<double> > state = args[0];
    AnyType tuple;

    tuple << state.mean << state.std;
    return tuple;
}

// ------------------------------------------------------------------------

AnyType utils_normalize_data::run (AnyType& args)
{
    MutableMappedColumnVector x(NULL);
    try {
        new (&x) MutableMappedColumnVector(args[0].getAs<MutableMappedColumnVector>());
    } catch(const ArrayWithNullException &e) {
       // warning("Input array contains NULL values, skipping this array normalization");
        return Null();
    }
    MappedColumnVector mean = args[1].getAs<MappedColumnVector>();
    MappedColumnVector std = args[2].getAs<MappedColumnVector>();

    for (int i = 0; i < x.size(); i++)
    {
        if(close_to_zero(std(i)))
            x(i) = x(i) - mean(i);
        else
            x(i) = (x(i) - mean(i)) / std(i);
    }

    return x;
}

}
}
}
