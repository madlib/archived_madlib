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
 * database with length at least 9, and at least first 3 elemenets
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
        // but I am not clear about the type binding/alignment
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
        return 8 + 2 * LMFModel<Handle>::modelSize(inRowDim, inColDim, inMaxRank);
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
     * - 5 + sizeOfModel: RMSE (root mean squared error)
     *
     * Intra-iteration components (updated in transition step):
     *   sizeOfModel = (rowDim + colDim) * maxRank
     * - 6 + sizeOfModel: numRows (number of rows processed in this iteration)
     * - 7 + sizeOfModel: loss (sum of squared errors)
     * - 8 + sizeOfModel: incrModel (volatile model for incrementally update)
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
        uint32_t sizeOfModel = LMFModel<Handle>::modelSize(task.rowDim,
                task.colDim, task.maxRank);
        task.RMSE.rebind(&mStorage[5 + sizeOfModel]);

        algo.numRows.rebind(&mStorage[6 + sizeOfModel]);
        algo.loss.rebind(&mStorage[7 + sizeOfModel]);
        algo.incrModel.matrixU.rebind(&mStorage[8 + sizeOfModel],
                task.rowDim, task.maxRank);
        algo.incrModel.matrixV.rebind(&mStorage[8 + sizeOfModel +
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

/**
 * @brief Inter- (Task State) and intra-iteration (Algo State) state of
 *        incremental gradient descent for multi-layer perceptron
 *
 * TransitionState encapsualtes the transition state during the
 * aggregate function during an iteration. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 6, and at least first elemenet
 * is 0 (exact values of other elements are ignored).
 *
 */
template <class Handle>
class MLPIGDState {
    template <class OtherHandle>
    friend class MLPIGDState;

public:
    MLPIGDState(const AnyType &inArray) : mStorage(inArray.getAs<Handle>()) {
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
    inline void allocate(const Allocator &inAllocator, 
            const uint16_t &inNumberOfStages, const uint16_t *inNumbersOfUnits) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(
                        arraySize(inNumberOfStages, inNumbersOfUnits));

        // This rebind is totally for the following lines of code to take
        // effect. I can also do something like "mStorage[0] = N",
        // but I am not clear about the type binding/alignment
        rebind();
        task.numberOfStages = inNumberOfStages;
        uint16_t N = inNumberOfStages;
        uint16_t k;
        for (k = 0; k <= N; k ++) {
            task.numbersOfUnits[k] = inNumbersOfUnits[k];
        }
        
        // This time all the member fields are correctly binded
        rebind();
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    MLPIGDState &operator=(const MLPIGDState<OtherHandle> &inOtherState) {
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

    static inline uint32_t arraySize(const uint16_t &inNumberOfStages,
            const uint16_t *inNumbersOfUnits) {
        uint32_t sizeOfModel =
            MLPModel<Handle>::modelSize(inNumberOfStages, inNumbersOfUnits);
        return 1                        // numberOfStages = N
            + (inNumberOfStages + 1)    // numbersOfUnits: size is (N + 1)
            + 1                         // stepsize
            + sizeOfModel               // model
            
            + 1                         // numRows
            + 1                         // loss
            + sizeOfModel;              // incrModel
    }

private:
    /**
     * @brief Rebind to a new storage array.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: numberOfStages (number of stages (layers), design doc: N)
     * - 1: numbersOfUnits (numbers of activation units, design doc: n_0,...,n_N)
     * - N + 2: stepsize (step size of gradient steps)
     * - N + 3: model (coefficients, design doc: u)
     *
     * Intra-iteration components (updated in transition step):
     *   sizeOfModel = # of entries in u, (\sum_1^N n_{k-1} n_k)
     * - N + 3 + sizeOfModel: numRows (number of rows processed in this iteration)
     * - N + 4 + sizeOfModel: loss (loss value, the sum of squared errors)
     * - N + 5 + sizeOfModel: incrModel (volatile model for incrementally update)
     */
    void rebind() {
        task.numberOfStages.rebind(&mStorage[0]);
        size_t N = task.numberOfStages;
        uint16_t k;

        task.numbersOfUnits = 
            reinterpret_cast<dimension_pointer_type >(&mStorage[1]);
        const uint16_t *n = task.numbersOfUnits;
        
        task.stepsize.rebind(&mStorage[N + 2]);

        task.model.u.clear();
        uint32_t sizeOfModel = 0;
        for (k = 1; k <= N; k ++) {
            task.model.u.push_back(Eigen::Map<Matrix >(
                    const_cast<double*>(
                    &mStorage[N + 3 + sizeOfModel]), n[k-1] + 1, n[k] + 1));
            sizeOfModel += (n[k-1] + 1) * (n[k] + 1);
        }

        algo.numRows.rebind(&mStorage[N + 3 + sizeOfModel]);
        algo.loss.rebind(&mStorage[N + 4 + sizeOfModel]);

        algo.incrModel.u.clear();
        uint32_t sizeOfIncrModel = 0;
        for (k = 1; k <= N; k ++) {
            algo.incrModel.u.push_back(Eigen::Map<Matrix >(
                    const_cast<double*>(
                        &mStorage[N + 5 + sizeOfModel + sizeOfIncrModel]),
                    n[k-1] + 1, n[k] + 1));
            sizeOfIncrModel += (n[k-1] + 1) * (n[k] + 1);
        }
    }

    Handle mStorage;

    typedef typename HandleTraits<Handle>::ReferenceToUInt16 dimension_type;
    typedef typename HandleTraits<Handle>::UInt16Ptr dimension_pointer_type;
    typedef typename HandleTraits<Handle>::ReferenceToUInt64 count_type;
    typedef typename HandleTraits<Handle>::ReferenceToDouble numeric_type;

public:
    struct TaskState {
        dimension_type numberOfStages;
        dimension_pointer_type numbersOfUnits;
        numeric_type stepsize;
        MLPModel<Handle> model;
    } task;

    struct AlgoState {
        count_type numRows;
        numeric_type loss;
        MLPModel<Handle> incrModel;
    } algo;
};

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

