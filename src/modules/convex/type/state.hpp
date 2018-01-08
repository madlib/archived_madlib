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
    inline void allocate(const Allocator &inAllocator, int32_t inRowDim,
            int32_t inColDim, int32_t inMaxRank) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(
                arraySize(inRowDim, inColDim, inMaxRank));

        // This rebind is for the following 3 lines of code to take
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

    static inline uint32_t arraySize(const int32_t inRowDim,
            const int32_t inColDim, const int32_t inMaxRank) {
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
        typename HandleTraits<Handle>::ReferenceToInt32 rowDim;
        typename HandleTraits<Handle>::ReferenceToInt32 colDim;
        typename HandleTraits<Handle>::ReferenceToInt32 maxRank;
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
        algo.gradient.setZero();
        algo.incrModel = task.model;
    }

    static inline uint32_t arraySize(const uint32_t inDimension) {
        return 4 + 3 * inDimension;
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
     * - 3 + dimension: loss (sum of loss for each row)
     * - 4 + dimension: gradient (sum of gradient for each row)
     * - 4 + dimension: incrModel (volatile model for incrementally update)
     */
    void rebind() {
        task.dimension.rebind(&mStorage[0]);
        task.stepsize.rebind(&mStorage[1]);
        task.model.rebind(&mStorage[2], task.dimension);

        algo.numRows.rebind(&mStorage[2 + task.dimension]);
        algo.loss.rebind(&mStorage[3 + task.dimension]);
        algo.gradient.rebind(&mStorage[4 + task.dimension], task.dimension);
        algo.incrModel.rebind(&mStorage[4 + task.dimension * 2], task.dimension);
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
            gradient;
        typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap
            incrModel;
    } algo;
};

template <class Handle>
class SVMMinibatchState {
    template <class OtherHandle>
    friend class SVMMinibatchState;

public:
    SVMMinibatchState(const AnyType &inArray) : mStorage(inArray.getAs<Handle>()) {
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
     * @brief Allocating the state.
     */
    inline void allocate(const Allocator &inAllocator, uint32_t nFeatures) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(nFeatures));

        rebind();
        task.nFeatures = nFeatures;
        rebind();
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    SVMMinibatchState &operator=(const SVMMinibatchState<OtherHandle> &inOtherState) {
        for (size_t i = 0; i < mStorage.size(); i++) {
            mStorage[i] = inOtherState.mStorage[i];
        }

        return *this;
    }

    static inline uint32_t arraySize(const uint32_t nFeatures) {
        return 8 + nFeatures;
    }

protected:
    void rebind() {
        task.nFeatures.rebind(&mStorage[0]);
        task.stepsize.rebind(&mStorage[1]);
        algo.numRows.rebind(&mStorage[2]);
        algo.loss.rebind(&mStorage[3]);
        task.reg.rebind(&mStorage[4]);
        algo.batchSize.rebind(&mStorage[5]);
        algo.nEpochs.rebind(&mStorage[6]);
        algo.numBuffers.rebind(&mStorage[7]);
        task.model.rebind(&mStorage[8], task.nFeatures);
    }

    Handle mStorage;

public:
    struct TaskState {
        typename HandleTraits<Handle>::ReferenceToUInt32 nFeatures;
        typename HandleTraits<Handle>::ReferenceToDouble stepsize;
        typename HandleTraits<Handle>::ReferenceToDouble reg;
        typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap model;
    } task;

    struct AlgoState {
        typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
        typename HandleTraits<Handle>::ReferenceToUInt64 numBuffers;
        typename HandleTraits<Handle>::ReferenceToDouble loss;
        typename HandleTraits<Handle>::ReferenceToUInt32 batchSize;
        typename HandleTraits<Handle>::ReferenceToUInt32 nEpochs;
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


/**
 * @brief Inter- (Task State) and intra-iteration (Algo State) state of
 *        incremental gradient descent for multilayer perceptron
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
                         const uint16_t &inNumberOfStages,
                         const double *inNumbersOfUnits) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(
                        arraySize(inNumberOfStages, inNumbersOfUnits));

        // This rebind is for the following lines of code to take
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
                                     const double *inNumbersOfUnits) {
        uint32_t sizeOfModel =
            MLPModel<Handle>::arraySize(inNumberOfStages, inNumbersOfUnits);
        return 1                        // numberOfStages = N
            + (inNumberOfStages + 1)    // numbersOfUnits: size is (N + 1)
            + 1                         // stepsize
            + 1                         // lambda
            + 1                         // is_classification
            + 1                         // activation
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
     * - N + 3: lambda (regularization term)
     * - N + 4: is_classification (do classification)
     * - N + 5: activation (activation function)
     * - N + 6: coeff (coefficients, design doc: u)
     *
     * Intra-iteration components (updated in transition step):
     *   sizeOfModel = # of entries in u + 2, (\sum_1^N n_{k-1} n_k)
     * - N + 6 + sizeOfModel: coeff (volatile model for incrementally update)
     * - N + 6 + 2*sizeOfModel: numRows (number of rows processed in this iteration)
     * - N + 7 + 2*sizeOfModel: loss (loss value, the sum of squared errors)
     */
    void rebind() {
        task.numberOfStages.rebind(&mStorage[0]);
        size_t N = task.numberOfStages;
        task.numbersOfUnits =
            reinterpret_cast<dimension_pointer_type>(&mStorage[1]);
        task.stepsize.rebind(&mStorage[N + 2]);
        task.lambda.rebind(&mStorage[N + 3]);
        uint32_t sizeOfModel = task.model.rebind(&mStorage[N + 4],&mStorage[N + 5],&mStorage[N + 6],
                task.numberOfStages, task.numbersOfUnits);

        algo.incrModel.rebind(&mStorage[N + 4],&mStorage[N + 5],&mStorage[N + 6 + sizeOfModel],
                task.numberOfStages, task.numbersOfUnits);
        algo.numRows.rebind(&mStorage[N + 6 + 2*sizeOfModel]);
        algo.loss.rebind(&mStorage[N + 7 + 2*sizeOfModel]);

    }

    Handle mStorage;

    typedef typename HandleTraits<Handle>::ReferenceToUInt16 dimension_type;
    typedef typename HandleTraits<Handle>::DoublePtr dimension_pointer_type;
    typedef typename HandleTraits<Handle>::ReferenceToUInt64 count_type;
    typedef typename HandleTraits<Handle>::ReferenceToDouble numeric_type;

public:
    struct TaskState {
        dimension_type numberOfStages;
        dimension_pointer_type numbersOfUnits;
        numeric_type stepsize;
        numeric_type lambda;
        MLPModel<Handle> model;
    } task;

    struct AlgoState {
        MLPModel<Handle> incrModel;
        count_type numRows;
        numeric_type loss;
    } algo;
};



/**
 * @brief Inter- (Task State) and intra-iteration (Algo State) state of
 *        incremental gradient descent for multilayer perceptron
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
class MLPMiniBatchState {
    template <class OtherHandle>
    friend class MLPMiniBatchState;

public:
    MLPMiniBatchState(const AnyType &inArray) : mStorage(inArray.getAs<Handle>()) {
        rebind();
    }

    /**
     * @brief Reset the intra-iteration fields.
     */
    inline void reset() {
        algo.numRows = 0;
        algo.numBuffers = 0;
        algo.loss = 0.;
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
                         const uint16_t &inNumberOfStages,
                         const double *inNumbersOfUnits) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(
                        arraySize(inNumberOfStages, inNumbersOfUnits));

        // This rebind is for the following lines of code to take
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
    MLPMiniBatchState &operator=(const MLPMiniBatchState<OtherHandle> &inOtherState) {
        for (size_t i = 0; i < mStorage.size(); i++) {
            mStorage[i] = inOtherState.mStorage[i];
        }

        return *this;
    }

    static inline uint32_t arraySize(const uint16_t &inNumberOfStages,
                                     const double *inNumbersOfUnits) {
        uint32_t sizeOfModel =
            MLPModel<Handle>::arraySize(inNumberOfStages, inNumbersOfUnits);
        return 1                        // numberOfStages = N
            + (inNumberOfStages + 1)    // numbersOfUnits: size is (N + 1)
            + 1                         // stepsize
            + 1                         // lambda
            + 1                         // is_classification
            + 1                         // activation

            + 1                         // numRows
            + 1                         // numBuffers
            + 1                         // batchSize
            + 1                         // nEpochs
            + 1                         // loss

            + sizeOfModel;              // model
    }

    Handle mStorage;
private:
    /**
     * @brief Rebind to a new storage array.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: numberOfStages (number of stages (layers), design doc: N)
     * - 1: numbersOfUnits (numbers of activation units, design doc: n_0,...,n_N)
     * - N + 2: stepsize (step size of gradient steps)
     * - N + 3: lambda (regularization term)
     * - N + 4: is_classification (do classification)
     * - N + 5: activation (activation function)
     *
     * Intra-iteration components (updated in transition step):
     *   sizeOfModel = # of entries in u, (\sum_1^N n_{k-1} n_k)
     * - N + 6 + sizeOfModel: coeff (volatile model for incrementally update)
     * - N + 6 + sizeOfModel: numRows (number of rows processed in this iteration)
     * - N + 6 + sizeOfModel: numBuffers (number of rows processed in this iteration)
     * - N + 6 + sizeOfModel: batchSize (number of rows processed in this iteration)
     * - N + 6 + sizeOfModel: nEpochs (number of rows processed in this iteration)
     * - N + 7 + 2*sizeOfModel: loss (loss value, the sum of squared errors)
     */
    void rebind() {

        task.numberOfStages.rebind(&mStorage[0]);
        size_t N = task.numberOfStages;

        task.numbersOfUnits =
            reinterpret_cast<dimension_pointer_type>(&mStorage[1]);
        task.stepsize.rebind(&mStorage[N + 2]);
        task.lambda.rebind(&mStorage[N + 3]);
        algo.numRows.rebind(&mStorage[N + 6]);
        algo.numBuffers.rebind(&mStorage[N + 7]);
        algo.batchSize.rebind(&mStorage[N + 8]);
        algo.nEpochs.rebind(&mStorage[N + 9]);
        algo.loss.rebind(&mStorage[N + 10]);
        size_t sizeOfModel = task.model.rebind(&mStorage[N + 4],
                          &mStorage[N + 5],
                          &mStorage[N + 11],
                          task.numberOfStages,
                          task.numbersOfUnits);
    }


    typedef typename HandleTraits<Handle>::ReferenceToUInt16 dimension_type;
    typedef typename HandleTraits<Handle>::DoublePtr dimension_pointer_type;
    typedef typename HandleTraits<Handle>::ReferenceToUInt64 count_type;
    typedef typename HandleTraits<Handle>::ReferenceToDouble numeric_type;

public:
    struct TaskState {
        dimension_type numberOfStages;
        dimension_pointer_type numbersOfUnits;
        numeric_type stepsize;
        numeric_type lambda;
        MLPModel<Handle> model;
    } task;

    struct AlgoState {
        count_type numRows;
        count_type numBuffers;
        dimension_type batchSize;
        dimension_type nEpochs;
        numeric_type loss;
    } algo;
};


} // namespace convex

} // namespace modules

} // namespace madlib

#endif

