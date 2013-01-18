/* ----------------------------------------------------------------------- *//**
 *
 * @file state.hpp
 *
 * This file contains definitions of user-defined aggregates.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TYPE_STATE_HPP_
#define MADLIB_MODULES_CONVEX_TYPE_STATE_HPP_

#include <dbconnector/dbconnector.hpp>
#include "model.hpp"

namespace madlib {

namespace modules {

namespace convex {

// use Eign
using namespace madlib::dbal::eigen_integration;

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
        task.RMSE = sqrt(algo.loss / static_cast<double>(algo.numRows));
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
        task.scaleFactor.rebind(&mStorage[4]);
        task.model.matrixU.rebind(&mStorage[5], task.rowDim, task.maxRank);
        task.model.matrixV.rebind(&mStorage[5 + task.rowDim * task.maxRank],
                task.colDim, task.maxRank);
        uint32_t modelLength = LMFModel<Handle>::arraySize(task.rowDim,
                task.colDim, task.maxRank);
        task.RMSE.rebind(&mStorage[5 + modelLength]);

        algo.numRows.rebind(&mStorage[6 + modelLength]);
        algo.loss.rebind(&mStorage[7 + modelLength]);
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
        typename HandleTraits<Handle>::ReferenceToDouble scaleFactor;
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
 *        incremental gradient descent for generalized linear models
 * 
 * Generalized Linear Models (GLMs): Logistic regression, Linear SVM
 *
 * TransitionState encapsualtes the transition state during the
 * aggregate function during an iteration. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and at least first elemenet is 0
 * (exact values of other elements are ignored).
 *
 */
template <class Handle>
class GLMIGDState {
    template <class OtherHandle>
    friend class GLMIGDState;

public:
    GLMIGDState(const AnyType &inArray) : mStorage(inArray.getAs<Handle>()) {
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
    inline void allocate(const Allocator &inAllocator, uint32_t inDimension) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inDimension));

        task.dimension.rebind(&mStorage[0]);
        task.dimension = inDimension;
        
        rebind();
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    GLMIGDState &operator=(const GLMIGDState<OtherHandle> &inOtherState) {
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

    static inline uint32_t arraySize(const uint32_t inDimension) {
        return 4 + 2 * inDimension;
    }

protected:
    /**
     * @brief Rebind to a new storage array.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: dimension (dimension of the model)
     * - 1: stepsize (step size of gradient steps)
     * - 2: model (coefficients)
     *
     * Intra-iteration components (updated in transition step):
     * - 2 + dimension: numRows (number of rows processed in this iteration)
     * - 3 + dimension: loss (sum of loss for each rows)
     * - 4 + dimension: incrModel (volatile model for incrementally update)
     */
    void rebind() {
        task.dimension.rebind(&mStorage[0]);
        task.stepsize.rebind(&mStorage[1]);
        task.model.rebind(&mStorage[2], task.dimension);

        algo.numRows.rebind(&mStorage[2 + task.dimension]);
        algo.loss.rebind(&mStorage[3 + task.dimension]);
        algo.incrModel.rebind(&mStorage[4 + task.dimension], task.dimension);
    }

    Handle mStorage;

public:
    struct TaskState {
        typename HandleTraits<Handle>::ReferenceToUInt32 dimension;
        typename HandleTraits<Handle>::ReferenceToDouble stepsize;
        typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap model;
    } task;

    struct AlgoState {
        typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
        typename HandleTraits<Handle>::ReferenceToDouble loss;
        typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap
            incrModel;
    } algo;
};

template <class Handle>
class RegularizedGLMIGDState {
    template <class OtherHandle>
    friend class RegularizedGLMIGDState;

public:
    RegularizedGLMIGDState(const AnyType &inArray) : mStorage(inArray.getAs<Handle>()) {
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
    inline void allocate(const Allocator &inAllocator, uint32_t inDimension) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inDimension));

        task.dimension.rebind(&mStorage[0]);
        task.dimension = inDimension;
        
        rebind();
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    RegularizedGLMIGDState &operator=(const RegularizedGLMIGDState<OtherHandle> &inOtherState) {
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

    static inline uint32_t arraySize(const uint32_t inDimension) {
        return 6 + 3 * inDimension;
    }

protected:
    /**
     * @brief Rebind to a new storage array.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: dimension (dimension of the model)
     * - 1: stepsize (step size of gradient steps)
     * - 2: lambda (regularization term)
     * - 3: totalRows (needed for amortizing lambda)
     * - 4: model (coefficients)
     *
     * Intra-iteration components (updated in transition step):
     * - 4 + dimension: numRows (number of rows processed in this iteration)
     * - 5 + dimension: loss (sum of loss for each rows)
     * - 6 + dimension: incrModel (volatile model for incrementally update)
     * - 6 + 2 * dimension: gradient (volatile temp variable to have model type)
     */
    void rebind() {
        task.dimension.rebind(&mStorage[0]);
        task.stepsize.rebind(&mStorage[1]);
        task.lambda.rebind(&mStorage[2]);
        task.totalRows.rebind(&mStorage[3]);
        task.model.rebind(&mStorage[4], task.dimension);

        algo.numRows.rebind(&mStorage[4 + task.dimension]);
        algo.loss.rebind(&mStorage[5 + task.dimension]);
        algo.incrModel.rebind(&mStorage[6 + task.dimension], task.dimension);
        algo.gradient.rebind(&mStorage[6 + 2 * task.dimension], task.dimension);
    }

    Handle mStorage;

public:
    struct TaskState {
        typename HandleTraits<Handle>::ReferenceToUInt32 dimension;
        typename HandleTraits<Handle>::ReferenceToDouble stepsize;
        typename HandleTraits<Handle>::ReferenceToDouble lambda;
        typename HandleTraits<Handle>::ReferenceToUInt64 totalRows;
        typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap model;
    } task;

    struct AlgoState {
        typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
        typename HandleTraits<Handle>::ReferenceToDouble loss;
        typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap
            incrModel;
        typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap
            gradient;
    } algo;
};

/**
 * @brief Inter- (Task State) and intra-iteration (Algo State) state of
 *        Conjugate Gradient for generalized linear models
 * 
 * Generalized Linear Models (GLMs): Logistic regression, Linear SVM
 *
 * TransitionState encapsualtes the transition state during the
 * aggregate function during an iteration. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and at least first elemenet is 0
 * (exact values of other elements are ignored).
 *
 */
template <class Handle>
class GLMCGState {
    template <class OtherHandle>
    friend class GLMCGState;

public:
    GLMCGState(const AnyType &inArray) : mStorage(inArray.getAs<Handle>()) {
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
    inline void allocate(const Allocator &inAllocator, uint32_t inDimension) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inDimension));

        task.dimension.rebind(&mStorage[0]);
        task.dimension = inDimension;
        
        rebind();
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    GLMCGState &operator=(const GLMCGState<OtherHandle> &inOtherState) {
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
        algo.incrGradient = ColumnVector::Zero(task.dimension);
    }

    static inline uint32_t arraySize(const uint32_t inDimension) {
        return 5 + 4 * inDimension;
    }

private:
    /**
     * @brief Rebind to a new storage array.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: dimension (dimension of the model)
     * - 1: iteration (current number of iterations executed)
     * - 2: stepsize (step size of gradient steps)
     * - 3: model (coefficients)
     * - 3 + dimension: direction (conjugate direction)
     * - 3 + 2 * dimension: gradient (gradient of loss functions)
     *
     * Intra-iteration components (updated in transition step):
     * - 3 + 3 * dimension: numRows (number of rows processed in this iteration)
     * - 4 + 3 * dimension: loss (sum of loss for each rows)
     * - 5 + 3 * dimension: incrGradient (volatile gradient for update)
     */
    void rebind() {
        task.dimension.rebind(&mStorage[0]);
        task.iteration.rebind(&mStorage[1]);
        task.stepsize.rebind(&mStorage[2]);
        task.model.rebind(&mStorage[3], task.dimension);
        task.direction.rebind(&mStorage[3 + task.dimension], task.dimension);
        task.gradient.rebind(&mStorage[3 + 2 * task.dimension], task.dimension);

        algo.numRows.rebind(&mStorage[3 + 3 * task.dimension]);
        algo.loss.rebind(&mStorage[4 + 3 * task.dimension]);
        algo.incrGradient.rebind(&mStorage[5 + 3 * task.dimension],
                task.dimension);
    }

    Handle mStorage;

public:
    typedef typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap
        TransparentColumnVector;

    struct TaskState {
        typename HandleTraits<Handle>::ReferenceToUInt32 dimension;
        typename HandleTraits<Handle>::ReferenceToUInt32 iteration;
        typename HandleTraits<Handle>::ReferenceToDouble stepsize;
        TransparentColumnVector model;
        TransparentColumnVector direction;
        TransparentColumnVector gradient;
    } task;

    struct AlgoState {
        typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
        typename HandleTraits<Handle>::ReferenceToDouble loss;
        TransparentColumnVector incrGradient;
    } algo;
};

/**
 * @brief Inter- (Task State) and intra-iteration (Algo State) state of
 *        Newton's method for generic objective functions (any tasks)
 *
 * This class assumes that the coefficients are of type vector. Low-rank 
 * matrix factorization, neural networks would not be able to use this.
 * 
 * TransitionState encapsualtes the transition state during the
 * aggregate function during an iteration. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and at least first elemenet is 0
 * (exact values of other elements are ignored).
 *
 */
template <class Handle>
class GLMNewtonState {
    template <class OtherHandle>
    friend class GLMNewtonState;

public:
    GLMNewtonState(const AnyType &inArray) : mStorage(inArray.getAs<Handle>()) {
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
    inline void allocate(const Allocator &inAllocator, uint16_t inDimension) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inDimension));

        task.dimension.rebind(&mStorage[0]);
        task.dimension = inDimension;
        
        rebind();
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    GLMNewtonState &operator=(const GLMNewtonState<OtherHandle> &inOtherState) {
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
        algo.gradient = ColumnVector::Zero(task.dimension);
        algo.hessian = Matrix::Zero(task.dimension, task.dimension);
    }

    static inline uint32_t arraySize(const uint16_t inDimension) {
        return 3 + (inDimension + 2) * inDimension;
    }

private:
    /**
     * @brief Rebind to a new storage array.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: dimension (dimension of the model)
     * - 1: model (coefficients)
     *
     * Intra-iteration components (updated in transition step):
     * - 1 + dimension: numRows (number of rows processed in this iteration)
     * - 2 + dimension: loss (sum of loss for each rows)
     * - 3 + dimension: gradient (volatile gradient for update)
     * - 3 + 2 * dimension: hessian (volatile hessian for update)
     */
    void rebind() {
        task.dimension.rebind(&mStorage[0]);
        task.model.rebind(&mStorage[1], task.dimension);

        algo.numRows.rebind(&mStorage[1 + task.dimension]);
        algo.loss.rebind(&mStorage[2 + task.dimension]);
        algo.gradient.rebind(&mStorage[3 + task.dimension], task.dimension);
        algo.hessian.rebind(&mStorage[3 + 2 * task.dimension], task.dimension,
                task.dimension);
    }

    Handle mStorage;

public:
    struct TaskState {
        typename HandleTraits<Handle>::ReferenceToUInt16 dimension;
        typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap model;
    } task;

    struct AlgoState {
        typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
        typename HandleTraits<Handle>::ReferenceToDouble loss;
        typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap gradient;
        typename HandleTraits<Handle>::MatrixTransparentHandleMap hessian;
    } algo;
};

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

