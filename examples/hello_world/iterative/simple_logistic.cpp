/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/* ------------------------------------------------------
 *
 * @file logistic.cpp
 *
 * @brief Logistic-Regression functions
 *
 * We implement the conjugate-gradient method and the iteratively-reweighted-
 * least-squares method.
 *
 *//* ----------------------------------------------------------------------- */
#include <limits>
#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>
#include <boost/math/distributions.hpp>
#include <modules/prob/student.hpp>
#include "simple_logistic.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

namespace hello_world {

// FIXME this enum should be accessed by all modules that may need grouping
// valid status values
enum { IN_PROCESS, COMPLETED, TERMINATED, NULL_EMPTY };

// Internal functions
AnyType stateToResult(const Allocator &inAllocator,
                      const HandleMap<const ColumnVector, TransparentHandle<double> >& inCoef,
                      const Matrix & hessian,
                      const double &logLikelihood,
                      int status,
                      const uint64_t &numRows);


// ---------------------------------------------------------------------------
//              Logistic Regression States
// ---------------------------------------------------------------------------

/**
 * @brief Inter- and intra-iteration state for conjugate-gradient method for
 *        logistic regression
 *
 * TransitionState encapsualtes the transition state during the
 * logistic-regression aggregate function. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elemenets are 0.
 *
 */
template <class Handle>
class LogRegrSPTransitionState {
    template <class OtherHandle>
    friend class LogRegrSPTransitionState;

  public:
    LogRegrSPTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[1]));
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
     * @brief Initialize the conjugate-gradient state.
     *
     * This function is only called for the first iteration, for the first row.
     */
    inline void initialize(const Allocator &inAllocator, uint16_t inWidthOfX) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                                             dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX));
        rebind(inWidthOfX);
        widthOfX = inWidthOfX;
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    LogRegrSPTransitionState &operator=(
        const LogRegrSPTransitionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    LogRegrSPTransitionState &operator+=(
        const LogRegrSPTransitionState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                                   "states");

        numRows += inOtherState.numRows;
        gradNew += inOtherState.gradNew;
        X_transp_AX += inOtherState.X_transp_AX;
        logLikelihood += inOtherState.logLikelihood;
        // merged state should have the higher status
        // (see top of file for more on 'status' )
        status = (inOtherState.status > status) ? inOtherState.status : status;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        X_transp_AX.fill(0);
        gradNew.fill(0);
        logLikelihood = 0;
        status = IN_PROCESS;
    }

  private:
    static inline size_t arraySize(const uint16_t inWidthOfX) {
        return 6 + inWidthOfX * inWidthOfX + 4 * inWidthOfX;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX The number of independent variables.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: iteration (current iteration)
     * - 1: widthOfX (number of coefficients)
     * - 2: coef (vector of coefficients)
     * - 2 + widthOfX: dir (direction)
     * - 2 + 2 * widthOfX: grad (gradient)
     * - 2 + 3 * widthOfX: beta (scale factor)
     *
     * Intra-iteration components (updated in transition step):
     * - 3 + 3 * widthOfX: numRows (number of rows already processed in this iteration)
     * - 4 + 3 * widthOfX: gradNew (intermediate value for gradient)
     * - 4 + 4 * widthOfX: X_transp_AX (X^T A X)
     * - 4 + widthOfX * widthOfX + 4 * widthOfX: logLikelihood ( ln(l(c)) )
     */
    void rebind(uint16_t inWidthOfX) {
        iteration.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        coef.rebind(&mStorage[2], inWidthOfX);
        dir.rebind(&mStorage[2 + inWidthOfX], inWidthOfX);
        grad.rebind(&mStorage[2 + 2 * inWidthOfX], inWidthOfX);
        beta.rebind(&mStorage[2 + 3 * inWidthOfX]);
        numRows.rebind(&mStorage[3 + 3 * inWidthOfX]);
        gradNew.rebind(&mStorage[4 + 3 * inWidthOfX], inWidthOfX);
        X_transp_AX.rebind(&mStorage[4 + 4 * inWidthOfX], inWidthOfX, inWidthOfX);
        logLikelihood.rebind(&mStorage[4 + inWidthOfX * inWidthOfX + 4 * inWidthOfX]);
        status.rebind(&mStorage[5 + inWidthOfX * inWidthOfX + 4 * inWidthOfX]);
    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt32 iteration;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap dir;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap grad;
    typename HandleTraits<Handle>::ReferenceToDouble beta;

    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap gradNew;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap X_transp_AX;
    typename HandleTraits<Handle>::ReferenceToDouble logLikelihood;
    typename HandleTraits<Handle>::ReferenceToUInt16 status;
};




/**
 * @brief Logistic function
 */
inline double sigma(double x) {
    return 1. / (1. + std::exp(-x));
}

/**
 * @brief Perform the logistic-regression transition step
 */
AnyType
logregr_simple_step_transition::run(AnyType &args) {
    LogRegrSPTransitionState<MutableArrayHandle<double> > state = args[0];
    if (args[1].isNull() || args[2].isNull()) { return args[0]; }
    double y = args[1].getAs<bool>() ? 1. : -1.;
    MappedColumnVector x;
    try {
        // an exception is raised in the backend if args[2] contains nulls
        MappedColumnVector xx = args[2].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    // The following check was added with MADLIB-138.
    if (!dbal::eigen_integration::isfinite(x)) {
        warning("Design matrix is not finite.");
        state.status = TERMINATED;
        return state;
    }

    if (state.numRows == 0) {
        if (x.size() > std::numeric_limits<uint16_t>::max()){
            warning("Number of independent variables cannot be larger than 65535.");
            state.status = TERMINATED;
            return state;
        }


        state.initialize(*this, static_cast<uint16_t>(x.size()));
        if (!args[3].isNull()) {
            LogRegrSPTransitionState<ArrayHandle<double> > previousState = args[3];

            state = previousState;
            state.reset();
        }
    }
    // Now do the transition step
    state.numRows++;
    double xc = dot(x, state.coef);
    state.gradNew.noalias() += sigma(-y * xc) * y * trans(x);

    // Note: sigma(-x) = 1 - sigma(x).
    double a = sigma(xc) * sigma(-xc);
    state.X_transp_AX += x * trans(x) * a;

    state.logLikelihood -= std::log( 1. + std::exp(-y * xc) );

    return state;
}


/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
logregr_simple_step_merge_states::run(AnyType &args) {
    LogRegrSPTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    LogRegrSPTransitionState<ArrayHandle<double> > stateRight = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.numRows == 0)
        return stateRight;
    else if (stateRight.numRows == 0)
        return stateLeft;

    // Merge states together and return
    stateLeft += stateRight;
    return stateLeft;
}



/**
 * @brief Perform the logistic-regression final step
 */
AnyType
logregr_simple_step_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    LogRegrSPTransitionState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0){
        state.status = NULL_EMPTY;
        return state;
    }

    // Note: k = state.iteration
    if (state.iteration == 0) {
        // Iteration computes the gradient

        state.dir = state.gradNew;
        state.grad = state.gradNew;
    } else {
        // We use the Hestenes-Stiefel update formula:
        //
        //            g_k^T (g_k - g_{k-1})
        // beta_k = -------------------------
        //          d_{k-1}^T (g_k - g_{k-1})
        ColumnVector gradNewMinusGrad = state.gradNew - state.grad;
        state.beta
            = dot(state.gradNew, gradNewMinusGrad)
            / dot(state.dir, gradNewMinusGrad);

        // Do a direction restart (Powell restart)
        // Note: This is testing whether state.beta < 0 if state.beta were
        // assigned according to Polak-Ribière
        if (dot(state.gradNew, gradNewMinusGrad)
            / dot(state.grad, state.grad) <= std::numeric_limits<double>::denorm_min()) state.beta = 0;

        // d_k = g_k - beta_k * d_{k-1}
        state.dir = state.gradNew - state.beta * state.dir;
        state.grad = state.gradNew;
    }

    // H_k = - X^T A_k X
    // where A_k = diag(a_1, ..., a_n) and a_i = sigma(x_i c_{k-1}) sigma(-x_i c_{k-1})
    //
    //             g_k^T d_k
    // alpha_k = -------------
    //           d_k^T H_k d_k
    //
    // c_k = c_{k-1} - alpha_k * d_k
    state.coef += dot(state.grad, state.dir) /
        as_scalar(trans(state.dir) * state.X_transp_AX * state.dir)
        * state.dir;

    if(!state.coef.is_finite()){
        // we don't throw an error - instead set the state status to allow
        // other states (possibly trained as part of group by) to continue
        // training
        warning("Over- or underflow in conjugate-gradient step, while updating "
              "coefficients. Input data is likely of poor numerical condition.");
        state.status = TERMINATED;
        return state;
    }

    state.iteration++;
    return state;
}

/**
 * @brief Return the difference in log-likelihood between two states
 */
AnyType
internal_logregr_simple_step_distance::run(AnyType &args) {
    LogRegrSPTransitionState<ArrayHandle<double> > stateLeft = args[0];
    LogRegrSPTransitionState<ArrayHandle<double> > stateRight = args[1];

    if(stateLeft.status == NULL_EMPTY || stateRight.status == NULL_EMPTY){
        return 0.0;
    }
    return std::abs(stateLeft.logLikelihood - stateRight.logLikelihood);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_logregr_simple_result::run(AnyType &args) {
    LogRegrSPTransitionState<ArrayHandle<double> > state = args[0];
    if (state.status == NULL_EMPTY)
        return Null();

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.X_transp_AX, EigenvaluesOnly, ComputePseudoInverse);

    return stateToResult(*this, state.coef,
                         state.X_transp_AX, state.logLikelihood,
                         state.status, state.numRows);
}


/**
 * @brief Compute the diagnostic statistics
 *
 * This function wraps the common parts of computing the results for both the
 * CG and the IRLS method.
 */
AnyType stateToResult(
    const Allocator &inAllocator,
    const HandleMap<const ColumnVector, TransparentHandle<double> > &inCoef,
    const Matrix & hessian,
    const double &logLikelihood,
    int status,
    const uint64_t &numRows) {

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        hessian, EigenvaluesOnly, ComputePseudoInverse);

    const Matrix &inverse_of_X_transp_AX = decomposition.pseudoInverse();
    const ColumnVector &diagonal_of_X_transp_AX = inverse_of_X_transp_AX.diagonal();

    MutableNativeColumnVector stdErr(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector waldZStats(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector waldPValues(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector oddsRatios(
        inAllocator.allocateArray<double>(inCoef.size()));

    for (Index i = 0; i < inCoef.size(); ++i) {
        stdErr(i) = std::sqrt(diagonal_of_X_transp_AX(i));
        waldZStats(i) = inCoef(i) / stdErr(i);
        waldPValues(i) = 2. * prob::cdf( prob::normal(),
                                         -std::abs(waldZStats(i)));
        oddsRatios(i) = std::exp( inCoef(i) );
    }

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << inCoef << logLikelihood << stdErr << waldZStats << waldPValues
          << oddsRatios << inverse_of_X_transp_AX
          << sqrt(decomposition.conditionNo()) << status << numRows;
    return tuple;
}

} // namespace hello_world
} // namespace modules
} // namespace madlib
