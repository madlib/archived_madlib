/* ----------------------------------------------------------------------- *//**
 *
 * @file crf.cpp
 *
 * @brief Logistic-Regression functions
 *
 * We implement the conjugate-gradient method and the iteratively-reweighted-
 * least-squares method.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <fstream>
#include "data.h"
#include "feature.h"
#include "featuregen.h"
#include "dictionary.h"
#include "option.h"
#include "doublevector.h"
#include "doublematrix.h"

#include "crf.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

namespace crf {

// Internal functions
AnyType stateToResult(const Allocator &inAllocator,
    const HandleMap<const ColumnVector, TransparentHandle<double> > &coef,
    const ColumnVector &diagonal_of_inverse_of_X_transp_AX,
    double logLikelihood,
    double conditionNo);

/**
 * @brief Inter- and intra-iteration state for conjugate-gradient method for
 *        logistic crfion
 *
 * TransitionState encapsualtes the transition state during the
 * logistic-crfion aggregate function. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elemenets are 0.
 *
 */
template <class Handle>
class GradientTransitionState {
    template <class OtherHandle>
    friend class GradientTransitionState;

public:
    GradientTransitionState(const AnyType &inArray)
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
    GradientTransitionState &operator=(
        const GradientTransitionState<OtherHandle> &inOtherState) {
        
        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }
    
    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    GradientTransitionState &operator+=(
        const GradientTransitionState<OtherHandle> &inOtherState) {
        
        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");
        
        numRows += inOtherState.numRows;
        gradNew += inOtherState.gradNew;
        X_transp_AX += inOtherState.X_transp_AX;
        logLikelihood += inOtherState.logLikelihood;
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
    }

private:
    static inline uint64_t arraySize(const uint16_t inWidthOfX) {
        return 5 + inWidthOfX * inWidthOfX + 4 * inWidthOfX;
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
    void rebind(uint32_t num_features, uint16_t num_labels) {
        iteration.rebind(&mStorage[0]);
        num_features.rebind(&mStorage[1]);
        num_labels.rebind(&mStorage[2]);
        loglikelihood.rebind(&mStorage[3]);
        gradlogli.rebind(&mStorage[4], num_features);
        grad_itermediate.rebind(&mStorage[4 + num_features], num_features);
        diag.rebind(&mStorage[4 + 2 * num_features], num_features);
        Mi.rebind(&mStorage[4 + 3 * num_features], num_labels * num_labels);
        Mi.rebind(&mStorage[4 + 3 * num_features + num_labels * num_labels], num_labels);
        alpa.rebind(&mStorage[4 + 3 * num_features + num_labels * num_labels + num_labels], num_labels);
        next_alpa.rebind(&mStorage[4 + 3 * num_features + num_labels * num_labels + 2 * num_labels], num_labels);
        temp.rebind(&mStorage[4 + 3 * num_features + num_labels * num_labels + 3 * num_labels], num_labels);
        ExpF.rebind(&mStorage[4 + 3 * num_features + num_labels * num_labels + 4 * num_labels], num_features);
        ws.rebind(&mStorage[4 + 5 * num_features + num_labels * num_labels + 4 * num_labels], num_features);
        iprint.rebind(&mStorage[4 + 6 * num_features + num_labels * num_labels + 4 * num_labels], 2);
        numRow.rebind(&mStorage[4 + 6 * num_features + num_labels * num_labels + 4 * num_labels +2 ], 3);
    }

    Handle mStorage;

public:
    // this control the status information reported during training

    typename HandleTraits<Handle>::ReferenceToUInt32 iteration;
    typename HandleTraits<Handle>::ReferenceToUInt32 num_features;
    typename HandleTraits<Handle>::ReferenceToUInt16 num_labels;
    typename HandleTraits<Handle>::ReferenceToDouble loglikelihood;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap gradlogli;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap grad_itermidiate;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap diag;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap Mi;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap Vi;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap alpha;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap next_alpha;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap temp;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap ExpF;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap ws;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap iprint;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
};


/**
 * @brief Perform the logistic-crfion transition step
 */
AnyType
linear_crf_step_transition::run(AnyType &args) {
    GradientTransitionState<MutableArrayHandle<double> > state = args[0];
    double y = args[1].getAs<bool>() ? 1. : -1.;
    HandleMap<const ColumnVector> x = args[2].getAs<ArrayHandle<double> >();
    
    // The following check was added with MADLIB-138.
    if (!isfinite(x))
        throw std::domain_error("Design matrix is not finite.");
    
    if (state.numRows == 0) {
        state.initialize(*this, x.size());
        if (!args[3].isNull()) {
            GradientTransitionState<ArrayHandle<double> > previousState = args[3];
            
            state = previousState;
            state.reset();
        }
    }
    
    // Now do the transition step
    state.numRows++;
    double xc = dot(x, state.coef);
    state.gradNew.noalias() += sigma(-y * xc) * y * trans(x);
    
    // Note: sigma(-x) = 1 - sigma(x).
    // a_i = sigma(x_i c) sigma(-x_i c)
    double a = sigma(xc) * sigma(-xc);
    triangularView<Lower>(state.X_transp_AX) += x * trans(x) * a;

    //          n
    //         --
    // l(c) = -\  log(1 + exp(-y_i * c^T x_i))
    //         /_
    //         i=1
    state.logLikelihood -= std::log( 1. + std::exp(-y * xc) );
    
    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
linear_crf_step_merge_states::run(AnyType &args) {
    GradientTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    GradientTransitionState<ArrayHandle<double> > stateRight = args[1];

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
 * @brief Perform the logistic-crfion final step
 */
AnyType
linear_crf_step_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    GradientTransitionState<MutableArrayHandle<double> > state = args[0];
    
    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    // Note: k = state.iteration
    if (state.iteration == 0) {
		// Iteration computes the gradient
	
		state.dir = state.gradNew;
		state.grad = state.gradNew;
	} else {
        ColumnVector gradNewMinusGrad = state.gradNew - state.grad;
        state.beta
            = dot(state.gradNew, gradNewMinusGrad)
            / dot(state.dir, gradNewMinusGrad);
        
        if (dot(state.gradNew, gradNewMinusGrad)
            / dot(state.grad, state.grad) < 0) state.beta = 0;
        
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
    
    if(!state.coef.is_finite())
        throw NoSolutionFoundException("Over- or underflow in "
            "conjugate-gradient step, while updating coefficients. Input data "
            "is likely of poor numerical condition.");
    //invole lbfgs algorithm
    lbfgs() 
    state.iteration++;
    return state;
}

/**
 * @brief Return the difference in log-likelihood between two states
 */
AnyType
internal_linear_crf_step_distance::run(AnyType &args) {
    GradientTransitionState<ArrayHandle<double> > stateLeft = args[0];
    GradientTransitionState<ArrayHandle<double> > stateRight = args[1];

    return std::abs(stateLeft.logLikelihood - stateRight.logLikelihood);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_linear_crf_result::run(AnyType &args) {
    GradientTransitionState<ArrayHandle<double> > state = args[0];
    
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.X_transp_AX, EigenvaluesOnly, ComputePseudoInverse);
        
    return stateToResult(*this, state.coef,
        decomposition.pseudoInverse().diagonal(), state.logLikelihood,
        decomposition.conditionNo());
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
    const ColumnVector &diagonal_of_inverse_of_X_transp_AX,
    double logLikelihood,
    double conditionNo) {
    
    // FIXME: We currently need to copy the coefficient to a native array
    // This should be transparent to user code
    HandleMap<ColumnVector> coef(
        inAllocator.allocateArray<double>(inCoef.size()));
    coef = inCoef;
    
    HandleMap<ColumnVector> stdErr(
        inAllocator.allocateArray<double>(coef.size()));
    HandleMap<ColumnVector> waldZStats(
        inAllocator.allocateArray<double>(coef.size()));
    HandleMap<ColumnVector> waldPValues(
        inAllocator.allocateArray<double>(coef.size()));
    HandleMap<ColumnVector> oddsRatios(
        inAllocator.allocateArray<double>(coef.size()));
    
    for (Index i = 0; i < coef.size(); ++i) {
        stdErr(i) = std::sqrt(diagonal_of_inverse_of_X_transp_AX(i));
        waldZStats(i) = coef(i) / stdErr(i);
        waldPValues(i) = 2. * normalCDF( -std::abs(waldZStats(i)) );
        oddsRatios(i) = std::exp( coef(i) );
    }
    
    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << coef << logLikelihood << stdErr << waldZStats << waldPValues
        << oddsRatios << conditionNo;
    return tuple;
}

} // namespace crf

} // namespace modules

} // namespace madlib
