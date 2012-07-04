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
//#include "mathlib.cpp"
#include "linear_crf_train.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

namespace crf {

// Internal functions
AnyType stateToResult(const Allocator &inAllocator,
                      const HandleMap<const ColumnVector, TransparentHandle<double> > &inlambda,
                      double loglikelihood);

/**
 * @brief Inter- and intra-iteration state for conjugate-gradient method for
 *        logistic crfion
 *
 * TransitionState encapsualtes the transition state during the
 * logistic-crfion aggregate function. To the database, the state is
 * std::exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
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

        if (mStorage.size() != inOtherState.mStorage.size())
            throw std::logic_error("Internal error: Incompatible transition "
                                   "states");

        numRows += inOtherState.numRows;
        grad_intermediate += inOtherState.grad_intermediate;
        loglikelihood += inOtherState.loglikelihood;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        grad_intermediate.fill(0);
        ExpF.fill(0);
        loglikelihood = 0;
    }

private:
    static inline uint64_t arraySize(const uint32_t num_features, const uint32_t num_labels) {
        return 4 + 6 * num_features + num_labels * num_labels + 4 * num_labels + 1;
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
     * - 4 + 3 * widthOfX: grad_intermediate (intermediate value for gradient)
     * - 4 + widthOfX * widthOfX + 4 * widthOfX: loglikelihood ( ln(l(c)) )
     */
    void rebind(uint32_t num_features, uint32_t num_labels) {
        iteration.rebind(&mStorage[0]);
        num_features.rebind(&mStorage[1]);
        num_labels.rebind(&mStorage[2]);
        loglikelihood.rebind(&mStorage[3]);
        gradlogli.rebind(&mStorage[4], num_features);
        grad_intermediate.rebind(&mStorage[4 + num_features], num_features);
        diag.rebind(&mStorage[4 + 2 * num_features], num_features);
        Mi.rebind(&mStorage[4 + 3 * num_features], num_labels * num_labels);
        Vi.rebind(&mStorage[4 + 3 * num_features + num_labels * num_labels], num_labels);
        alpha.rebind(&mStorage[4 + 3 * num_features + num_labels * num_labels + num_labels], num_labels);
        next_alpha.rebind(&mStorage[4 + 3 * num_features + num_labels * num_labels + 2 * num_labels], num_labels);
        temp.rebind(&mStorage[4 + 3 * num_features + num_labels * num_labels + 3 * num_labels], num_labels);
        ExpF.rebind(&mStorage[4 + 3 * num_features + num_labels * num_labels + 4 * num_labels], num_features);
        ws.rebind(&mStorage[4 + 5 * num_features + num_labels * num_labels + 4 * num_labels], num_features);
        numRows.rebind(&mStorage[4 + 6 * num_features + num_labels * num_labels + 4 * num_labels +1 ]);
    }

    Handle mStorage;

public:
    // this control the status information reported during training

    typename HandleTraits<Handle>::ReferenceToUInt32 iteration;
    typename HandleTraits<Handle>::ReferenceToUInt32 num_features;
    typename HandleTraits<Handle>::ReferenceToUInt16 num_labels;
    typename HandleTraits<Handle>::ReferenceToDouble loglikelihood;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap gradlogli;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap grad_intermediate;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap diag;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap Mi;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap Vi;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap alpha;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap next_alpha;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap temp;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap ExpF;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap ws;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
};


/**
 * @brief Perform the logistic-crfion transition step
 */
AnyType
linear_crf_step_transition::run(AnyType &args) {
    GradientTransitionState<MutableArrayHandle<double> > state = args[0];
    HandleMap<const ColumnVector> features = args[1].getAs<ArrayHandle<uint32_t> >();
    HandleMap<const ColumnVector> featureType = args[2].getAs<ArrayHandle<uint32_t> >();
    HandleMap<const ColumnVector> prevLabel = args[3].getAs<ArrayHandle<uint32_t> >();
    HandleMap<const ColumnVector> currLabel = args[4].getAs<ArrayHandle<uint32_t> >();
    uint32_t seq_len = args[3].getAs<uint32_t>();

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
    *alpha = 1;
    int betassize = betas.size();
    if (betassize < seq_len) {
        // allocate more beta vector
        for (i = 0; i < seq_len - betassize; i++) {
            betas.push_back(new doublevector(num_labels));
        }
    }
    int scalesize = scale.size();
    if (scalesize < seq_len) {
        // allocate more scale elements
        for (i = 0; i < seq_len - scalesize; i++) {
            scale.push_back(1.0);
        }
    }
    // compute beta values in a backward fashion
    // also scale beta-values to 1 to avoid numerical problems
    scale[seq_len - 1] = (popt->is_scaling) ? state.num_labels : 1;
    betas[seq_len - 1]->assign(1.0 / scale[seq_len - 1]);

    size_t index=0; 
    for (size_t i = seq_len - 1; i > 0; i--) {
        compute_log_Mi(state.features,state.featureType,state.Mi,state.Vi,state.lambda,index,state.num_labels,true);
        *temp = *(betas[i]);
        temp->comp_multstate.Vi;
        mathlib::mult(state.num_labels, betas[i - 1], Mi, temp, 0);
        // scale for the next (backward) beta values
        scale[i - 1] = (popt->is_scaling) ? betas[i - 1]->sum() : 1;
        betas[i - 1]->comp_mult(1.0 / scale[i - 1]);
    } // end of beta values computation

    // start to compute the log-likelihood of the current sequence
    for (j = 0; j < seq_len; j++) {
        compute_log_Mi(state.features,state.featureType,state.Mi,state.Vi,state.lambda,index,state.num_labels,true);
        if (j > 0) {
            *temp = *alpha;
            mathlib::mult(num_labels, state.next_alpha, Mi, temp, 1);
            state.next_alpha->comp_multstate.Vi;
        } else {
            *next_alpha = *Vi;
        }

        while (pfgen->has_next_feature()) {
        size_t f_index = state.features(index);
        size_t prev_index = state.prevLable(index);
        size_t curr_index = statecurrLable(index);
        size_t f_type =  state.featureType(index);
            if (f_type == 0 || f_type == 1) {
                state.gradlogli(index) += f.val;
                seq_logli += lambda(f_index) * f.val;
            }
            if (f_type == 1) {
                state.ExpF(f_index) += state.next_alpha(curr_label) * f.val * (*(betas[j]))[curr_label(index)];
            } else if (f_type == 0) {
                state.ExpF(f_index) += state.alpha[prev_label] * state.Vi(curr_index) * state.Mi(prev_index,curr_index)
                                     * f.val * (*(betas[j]))(curr_index);
            }
        }
        *alpha = *next_alpha;
        alpha->comp_mult(1.0 / scale[j]);
    }

    // Zx = sum(alpha_i_n) where i = 1..num_labels, n = seq_len
    double Zx = alpha->sum();
    state.grad_intermediate -= std::log(Zx);

    // re-correct the value of seq_logli because Zx was computed from
    // scaled alpha values
    for (size_t k = 0; k < seq_len; k++) {
        state.grad_intermediate -= std::log(scale[k]);
    }
    // update the gradient vector
    for (k = 0; k < state.num_features; k++) {
        grad_intermediate[k] -= ExpF[k] / Zx;
    }

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

    for (i = 0; i < state.num_features; i++) {
        gradlogli[i] = -1 * lambda[i] / popt->sigma_square;
        logli -= (lambda[i] * lambda[i]) / (2 * popt->sigma_square);
     }
        //invole lbfgs algorithm
        lbfgs(&state.num_features,&state.m_for_hessian,state.lambda,&state.loglikelihood,state.gradlogli,&statediagco,
              state.diag,state.eps_for_convergence,&state.xtol,state.ws,&state.iflog);
        // checking after calling LBFGS
        if (state.iflag < 0) {
            // LBFGS error
            printf("LBFGS routine encounters an error\n");
            if (is_logging) {
                fprintf(fout, "LBFGS routine encounters an error\n");
            }
            break;
        }
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
        return std::abs(stateLeft.loglikelihood - stateRight.loglikelihood);
    }

    /**
     * @brief Return the coefficients and diagnostic statistics of the state
     */
    AnyType
    internal_linear_crf_result::run(AnyType &args) {
        GradientTransitionState<ArrayHandle<double> > state = args[0];
        return stateToResult(*this, state.lambda, state.loglikelihood);
    }

    /**
     * @brief Compute the diagnostic statistics
     *
     * This function wraps the common parts of computing the results for both the
     * CG and the IRLS method.
     */
    AnyType stateToResult(
        const Allocator &inAllocator,
        const HandleMap<const ColumnVector, TransparentHandle<double> > &inlambda,
        double loglikelihood) {
        // FIXME: We currently need to copy the coefficient to a native array
        // This should be transparent to user code
        HandleMap<ColumnVector> lambda(
            inAllocator.allocateArray<double>(inCoef.size()));
        lambda = inlambda;

        // Return all coefficients, standard errors, etc. in a tuple
        AnyType tuple;
        tuple << loglikelihood << lambda;
        return tuple;
    }

    // compute log Mi (first-order Markov)
    AnyType compute_log_Mi(
    const HandleMap<const ColumnVector, TransparentHandle<double> > &features,
    const HandleMap<const ColumnVector, TransparentHandle<double> > &featureType,
    HandleMap<const ColumnVector, TransparentHandle<double> > &Mi,
    HandleMap<const ColumnVector, TransparentHandle<double> > &Vi,
    const HandleMap<const ColumnVector, TransparentHandle<double> > &lambda,
    uint32_t &index, uint32_t num_labels, const bool is_exp) {
    Mi.fill(0) = 0.0;
    Vi.fill(0) = 0.0;
    // examine all features at position "pos"
    while (features(index)!=-1) {
        size_t f_index = features(index);
        size_t prev_index = prevLable(index);
        size_t curr_index = currLable(index);
        size_t f_type =  featureType(index);
	if (f_type == 0) {
	    // state feature
	    Vi(curr_index) += lambda(f_index);
	} else if (f_type == 1) /* if (pos > 0)*/ {
	    Mi(prev_index, curr_index) += lambda(f_index);
	}
        index++;
    }
    
    // take exponential operator
    if (is_exp) {
	for (size_t i = 0; i < num_labels; i++) {
	    // update for Vi
	    Vi(i) = std::exp(Vi(i));
	    // update for Mi
	    for (size_t j = 0; j < num_labels; j++) {
		Mi(i, j) = std::exp(Mi(i, j));
	    }
	}
    }
}

}

} // namespace crf

} // namespace modules

} // namespace madlib
