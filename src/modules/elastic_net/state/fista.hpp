
/**
   @file fista.hpp

   This file contains the definitions for FISTA state of
   user-defined aggreagtes.
*/

#ifndef MADLIB_MODULES_ELASIC_NET_STATE_FISTA_
#define MADLIB_MODULES_ELASIC_NET_STATE_FISTA_

#include "dbconnector/dbconnector.hpp"
#include "modules/shared/HandleTraits.hpp"
// #include "convex/type/model.hpp"

namespace madlib {
namespace modules {
namespace elastic_net {

using namespace madlib::dbal::eigen_integration;

template <class Handle>
class FistaState
{
    template <class OtherHandle> friend class FistaState;

  public:
    FistaState (const AnyType& inArray):
        mStorage(inArray.getAs<Handle>())
    {
        rebind();
    }

    /**
       @brief Convert to backend representation

       Define this function so that we can use State in the argument
       list and as a return type.
    */
    inline operator AnyType () const
    {
        return mStorage;
    }

    /**
       @brief Allocating the needed memory blocks
    */
    inline void allocate (const Allocator& inAllocator,
                          uint32_t inDimension)
    {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inDimension));
        dimension.rebind(&mStorage[0]);
        dimension = inDimension;
        rebind();
    }

    /**
       @brief We need to support assigning the previous state
    */
    template <class OtherHandle>
    FistaState& operator= (const FistaState<OtherHandle>& inOtherState)
    {
        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
       @brief Total size of the state object
    */
    static inline uint32_t arraySize (const uint32_t inDimension)
    {
        return 22 + 4 * inDimension;
    }

  protected:
    void rebind ()
    {
        dimension.rebind(&mStorage[0]);
        lambda.rebind(&mStorage[1]);
        alpha.rebind(&mStorage[2]);
        is_active.rebind(&mStorage[3]);
        totalRows.rebind(&mStorage[4]);
        intercept.rebind(&mStorage[5]);
        intercept_y.rebind(&mStorage[6]);
        coef.rebind(&mStorage[7], dimension);
        coef_y.rebind(&mStorage[7 + dimension], dimension);
        // xmean.rebind(&mStorage[7 + 2 * dimension], dimension);
        // ymean.rebind(&mStorage[7 + 3 * dimension]);
        tk.rebind(&mStorage[7 + 2 * dimension]);
        numRows.rebind(&mStorage[8 + 2 * dimension]);
        gradient.rebind(&mStorage[9 + 2 * dimension], dimension);
        max_stepsize.rebind(&mStorage[9 + 3 * dimension]);
        eta.rebind(&mStorage[10 + 3 * dimension]);
        fn.rebind(&mStorage[11 + 3 * dimension]);
        Qfn.rebind(&mStorage[12 + 3 * dimension]);
        stepsize.rebind(&mStorage[13 + 3 * dimension]);
        b_coef.rebind(&mStorage[14 + 3 * dimension], dimension);
        b_intercept.rebind(&mStorage[14 + 4 * dimension]);
        use_active_set.rebind(&mStorage[15 + 4 * dimension]);
        iter.rebind(&mStorage[16 + 4 * dimension]);
        stepsize_sum.rebind(&mStorage[17 + 4 * dimension]);
        gradient_intercept.rebind(&mStorage[18 + 4 * dimension]);
        random_stepsize.rebind(&mStorage[19 + 4 * dimension]);
        backtracking.rebind(&mStorage[20 + 4 * dimension]);
        loglikelihood.rebind(&mStorage[21 + 4 * dimension]);
    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt32 dimension;
    typename HandleTraits<Handle>::ReferenceToDouble lambda;
    typename HandleTraits<Handle>::ReferenceToDouble alpha;
    typename HandleTraits<Handle>::ReferenceToUInt32 is_active; // is active-set being used now?
    typename HandleTraits<Handle>::ReferenceToUInt64 totalRows;
    typename HandleTraits<Handle>::ReferenceToDouble intercept;
    typename HandleTraits<Handle>::ReferenceToDouble intercept_y;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef_y;
    // typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap xmean;
    // typename HandleTraits<Handle>::ReferenceToDouble ymean;
    typename HandleTraits<Handle>::ReferenceToDouble tk;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap gradient;
    typename HandleTraits<Handle>::ReferenceToDouble max_stepsize;
    typename HandleTraits<Handle>::ReferenceToDouble eta;
    typename HandleTraits<Handle>::ReferenceToDouble fn; // store the function value in backtracking
    typename HandleTraits<Handle>::ReferenceToDouble Qfn; // the Q function value in backtracking
    typename HandleTraits<Handle>::ReferenceToDouble stepsize;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap b_coef; // backtracking coef
    typename HandleTraits<Handle>::ReferenceToDouble b_intercept; // backtracking intercept
    typename HandleTraits<Handle>::ReferenceToUInt32 use_active_set; // whether to use active set method
    typename HandleTraits<Handle>::ReferenceToUInt32 iter; // how many effective iteration run
    typename HandleTraits<Handle>::ReferenceToDouble stepsize_sum; // sum of step size so far
    typename HandleTraits<Handle>::ReferenceToDouble gradient_intercept; // gradient element for intercept
    typename HandleTraits<Handle>::ReferenceToUInt32 random_stepsize;
    typename HandleTraits<Handle>::ReferenceToUInt32 backtracking; // is backtracking now?
    typename HandleTraits<Handle>::ReferenceToDouble loglikelihood;  // loglk for previous iteration
};

}
}
}

#endif
