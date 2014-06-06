/**
   @file igd.hpp

   This file contains the definitions for IGD state of
   user-defined aggregates
*/

#ifndef MADLIB_MODULES_ELASIC_NET_STATE_IGD_
#define MADLIB_MODULES_ELASIC_NET_STATE_IGD_

#include "dbconnector/dbconnector.hpp"
#include "modules/shared/HandleTraits.hpp"

namespace madlib {
namespace modules {
namespace elastic_net {

using namespace madlib::dbal::eigen_integration;

template <class Handle>
class IgdState
{
    template <class OtherHandle> friend class IgdState;

  public:
    IgdState (const AnyType& inArray):
        mStorage(inArray.getAs<Handle>())
    {
        rebind();
    }

    /**
     * @brief Convert to backend representation
     *
     * We define this function so that we can use State in the
     * argument list and as a return type.
     */
    inline operator AnyType() const {
        return mStorage;
    }

    /**
     * @brief Allocating the incremental gradient state.
     */
    inline void allocate(const Allocator& inAllocator, uint32_t inDimension)
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

    /**
     * @brief Support for assigning the previous state
     */
    template <class OtherHandle>
    IgdState &operator= (const IgdState<OtherHandle>& inOtherState)
    {
        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Total size of the state object
     */
    static inline uint32_t arraySize (const uint32_t inDimension)
    {
        return 14 + 3 * inDimension;
    }

  protected:
    void rebind ()
    {
        dimension.rebind(&mStorage[0]);
        stepsize.rebind(&mStorage[1]);
        lambda.rebind(&mStorage[2]);
        alpha.rebind(&mStorage[3]);
        totalRows.rebind(&mStorage[4]);
        intercept.rebind(&mStorage[5]);
        ymean.rebind(&mStorage[6]);
        numRows.rebind(&mStorage[7]);
        loss.rebind(&mStorage[8]);
        p.rebind(&mStorage[9]);
        q.rebind(&mStorage[10]);
        xmean.rebind(&mStorage[11], dimension);
        coef.rebind(&mStorage[11 + dimension], dimension);
        theta.rebind(&mStorage[11 + 2 * dimension], dimension);
        threshold.rebind(&mStorage[11 + 3 * dimension]);
        step_decay.rebind(&mStorage[12 + 3 * dimension]);
        loglikelihood.rebind(&mStorage[13 + 3 * dimension]);

    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt32 dimension;
    typename HandleTraits<Handle>::ReferenceToDouble stepsize;
    typename HandleTraits<Handle>::ReferenceToDouble lambda; // regularization control
    typename HandleTraits<Handle>::ReferenceToDouble alpha; // elastic net control
    typename HandleTraits<Handle>::ReferenceToUInt64 totalRows;
    typename HandleTraits<Handle>::ReferenceToDouble intercept;
    typename HandleTraits<Handle>::ReferenceToDouble ymean;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap xmean;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToDouble loss;
    typename HandleTraits<Handle>::ReferenceToDouble p; // used for mirror truncation
    typename HandleTraits<Handle>::ReferenceToDouble q; // used for mirror truncation
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap theta; // used for mirror truncation
    typename HandleTraits<Handle>::ReferenceToDouble threshold; // used for remove tiny values
    typename HandleTraits<Handle>::ReferenceToDouble step_decay; // decay of step size
    typename HandleTraits<Handle>::ReferenceToDouble loglikelihood;  // loglk for previous iteration
};

}
}
}

#endif
