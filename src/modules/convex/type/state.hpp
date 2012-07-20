/* ----------------------------------------------------------------------- *//**
 *
 * @file state.hpp
 *
 * This file contains definitions of user-defined aggregates.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TYPE_STATE_HPP_
#define MADLIB_MODULES_CONVEX_TYPE_STATE_HPP_

#include "model.hpp"

namespace madlib {

namespace modules {

namespace convex {

/**
 * @brief Inter- (Task State) and intra-iteration (Algo State) state of
 *        incremental gradient descent for low-rank matrix factorization
 *
 * TransitionState encapsualtes the transition state during the
 * aggregate function during an iteration. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 8 (actually 9), and at least first 3 elemenets
 * are 0 (exact values of other elements are ignored).
 *
 */
template <class Handle>
class LMFIGDState {
    template <class OtherHandle>
    friend class LMFIGDState;

public:
    LMFIGDState(const AnyType &inArray) : mStorage(inArray.getAs<Handle>()) {
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
    inline void allocate(const Allocator &inAllocator, uint16_t inRowDim,
            uint16_t inColDim, uint16_t inMaxRank) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(
                arraySize(inRowDim, inColDim, inMaxRank));

        // This rebind is totally for the following 3 lines of code to take
        // effect. I can also do something like "mStorage[0] = inRowDim",
        // but I am not clear about the type casting
        rebind();
        task.rowDim = inRowDim;
        task.colDim = inColDim;
        task.maxRank = inMaxRank;
        
        // This time all the member fields are correctly binded
        rebind();
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    LMFIGDState &operator=(const LMFIGDState<OtherHandle> &inOtherState) {
        for (size_t i = 0; i < mStorage.size(); i++) {
            mStorage[i] = inOtherState.mStorage[i];
        }

        return *this;
    }

    /**
     * @brief Reset the intra-iteration fields.
     */
    inline void reset() {
        algo.numRows = 0;
        algo.loss = 0.;
        algo.incrModel = task.model;
    }

    /**
     * @brief Compute RMSE using loss and numRows
     *
     * This is the only function in this class that actually does
     * something for the convex programming; therefore, it looks a bit weird to
     * me. But I am not sure where I can move this to...
     */
    inline void computeRMSE() {
        task.RMSE = sqrt(algo.loss / algo.numRows);
    }

    static inline uint32_t arraySize(const uint16_t inRowDim, 
            const uint16_t inColDim, const uint16_t inMaxRank) {
        return 8 + 2 * LMFModel<Handle>::arraySize(inRowDim, inColDim, inMaxRank);
    }

private:
    /**
     * @brief Rebind to a new storage array.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: rowDim (row dimension of the input sparse matrix A)
     * - 1: colDim (col dimension of the input sparse matrix A)
     * - 2: maxRank (the rank of the low-rank assumption)
     * - 3: stepsize (step size of gradient steps)
     * - 4: initValue (value scale used to initialize the model)
     * - 5: model (matrices U(rowDim x maxRank), V(colDim x maxRank), A ~ UV')
     * - 5 + modelLength: RMSE (root mean squared error)
     *
     * Intra-iteration components (updated in transition step):
     *   modelLength = (rowDim + colDim) * maxRank
     * - 6 + modelLength: numRows (number of rows processed in this iteration)
     * - 7 + modelLength: loss (sum of squared errors)
     * - 8 + modelLength: incrModel (volatile model for incrementally update)
     */
    void rebind() {
        task.rowDim.rebind(&mStorage[0]);
        task.colDim.rebind(&mStorage[1]);
        task.maxRank.rebind(&mStorage[2]);
        task.stepsize.rebind(&mStorage[3]);
        task.initValue.rebind(&mStorage[4]);
        task.model.matrixU.rebind(&mStorage[5], task.rowDim, task.maxRank);
        task.model.matrixV.rebind(&mStorage[5 + task.rowDim * task.maxRank],
                task.colDim, task.maxRank);
        uint32_t modelLength = LMFModel<Handle>::arraySize(task.rowDim,
                task.colDim, task.maxRank);
//        task.model.rebind(&mStorage[5], task.rowDim,
//                task.colDim, task.maxRank);
        task.RMSE.rebind(&mStorage[5 + modelLength]);

        algo.numRows.rebind(&mStorage[6 + modelLength]);
        algo.loss.rebind(&mStorage[7 + modelLength]);
//        algo.incrModel.rebind(&mStorage[8 + modelLength], task.rowDim,
//                task.colDim, task.maxRank);
        algo.incrModel.matrixU.rebind(&mStorage[8 + modelLength],
                task.rowDim, task.maxRank);
        algo.incrModel.matrixV.rebind(&mStorage[8 + modelLength +
                task.rowDim * task.maxRank], task.colDim, task.maxRank);
    }

    Handle mStorage;

public:
    struct TaskState {
        typename HandleTraits<Handle>::ReferenceToUInt16 rowDim;
        typename HandleTraits<Handle>::ReferenceToUInt16 colDim;
        typename HandleTraits<Handle>::ReferenceToUInt16 maxRank;
        typename HandleTraits<Handle>::ReferenceToDouble stepsize;
        typename HandleTraits<Handle>::ReferenceToDouble initValue;
        LMFModel<Handle> model;
        typename HandleTraits<Handle>::ReferenceToDouble RMSE;
    } task;

    struct AlgoState {
        typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
        typename HandleTraits<Handle>::ReferenceToDouble loss;
        LMFModel<Handle> incrModel;
    } algo;
};

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

