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
                      const HandleMap<const ColumnVector, TransparentHandle<double> > &incoef,
                      double loglikelihood);

template <class Handle>
class LinCrfCGTransitionState {
    template <class OtherHandle>
    friend class LinCrfCGTransitionState;

public:
    LinCrfCGTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[1]),static_cast<uint16_t>(mStorage[2]));
    }

    inline operator AnyType() const {
        return mStorage;
    }

    inline void initialize(const Allocator &inAllocator, uint16_t num_features, uint16_t num_labels) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
        dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(num_features,num_labels));
        rebind(num_features,num_labels);
    }

    template <class OtherHandle>
    LinCrfCGTransitionState &operator=(
        const LinCrfCGTransitionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    template <class OtherHandle>
    LinCrfCGTransitionState &operator+=(
        const LinCrfCGTransitionState<OtherHandle> &inOtherState) {
        if (mStorage.size() != inOtherState.mStorage.size())
            throw std::logic_error("Internal error: Incompatible transition "
                                   "states");
        numRows += inOtherState.numRows;
        grad_new += inOtherState.grad_new;
        loglikelihood += inOtherState.loglikelihood;
        return *this;
    }

    inline void reset() {
        numRows = 0;
        grad_new.fill(0);
        loglikelihood = 0;
    }

private:
    static inline uint64_t arraySize(const uint32_t num_features) {
        return 5 + 5 * num_features;
    }

    void rebind(uint32_t inWidthOfFeature, uint32_t inWidthOfLabel) {
        iteration.rebind(&mStorage[0]);
        num_features.rebind(&mStorage[1]);
        num_labels.rebind(&mStorage[2]);
        coef.rebind(&mStorage[3], inWidthOfFeature);
        dir.rebind(&mStorage[3 + inWidthOfFeature], inWidthOfFeature);
        grad.rebind(&mStorage[3 + 2 * inWidthOfFeature], inWidthOfFeature);
        beta.rebind(&mStorage[3 + 3 * inWidthOfFeature]);
        numRows.rebind(&mStorage[4 + 3 * inWidthOfFeature]);
        gradNew.rebind(&mStorage[5 + 3 * inWidthOfFeature], inWidthOfFeature);
        loglikelihood.rebind(&mStorage[5 + 4 * inWidthOfFeature]);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt32 iteration;
    typename HandleTraits<Handle>::ReferenceToUInt32 num_features;
    typename HandleTraits<Handle>::ReferenceToUInt16 num_labels;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap dir;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap grad;
    typename HandleTraits<Handle>::ReferenceToDouble beta;

    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap grad_new;
    typename HandleTraits<Handle>::ReferenceToDouble loglikelihood;
};

AnyType
lincrf_cg_step_transition::run(AnyType &args) {
    LinCrfCGTransitionState<MutableArrayHandle<double> > state = args[0];
    HandleMap<const ColumnVector> features = args[1].getAs<ArrayHandle<double> >();
    HandleMap<const ColumnVector> featureType = args[2].getAs<ArrayHandle<double> >();
    HandleMap<const ColumnVector> prevLabel = args[3].getAs<ArrayHandle<double> >();
    HandleMap<const ColumnVector> currLabel = args[4].getAs<ArrayHandle<double> >();
    size_t seq_len = args[5].getAs<double>();
    if (state.numRows == 0) {
        state.initialize(*this, state.num_features, state.num_labels);
        if (!args[3].isNull()) {
            LinCrfCGTransitionState<ArrayHandle<double> > previousState = args[3];
            state = previousState;
            state.reset();
        }
    }
    Eigen::MatrixXd betas(seq_len,state.num_labels);
    Eigen::VectorXd scale(seq_len);
    Eigen::VectorXd Mi(state.num_labels,state.num_labels);
    Eigen::VectorXd Vi(state.num_labels);
    Eigen::VectorXd alpha(state.num_labels);
    Eigen::VectorXd next_alpha(state.num_labels);
    Eigen::VectorXd temp(state.num_labels);
    Eigen::VectorXd ExpF(state.num_features);
    // Now do the transition step
    state.numRows++;
    alpha.fill(1);
    ExpF.fill(0);
    // compute beta values in a backward fashion
    // also scale beta-values to 1 to avoid numerical problems
    scale(seq_len - 1) = state.num_labels;
    betas.row(seq_len - 1).fill(1.0 / scale(seq_len - 1));

    size_t index=0;
    for (size_t i = seq_len - 1; i > 0; i--) {
        Mi.fill(0);
        Vi.fill(0);
        // examine all features at position "pos"
        while (features(index)!=-1) {
            size_t f_index = features(index);
            size_t prev_index = prevLabel(index);
            size_t curr_index = currLabel(index);
            size_t f_type =  featureType(index);
            if (f_type == 0) {
                // state feature
                Vi(curr_index) += state.coef(f_index);
            } else if (f_type == 1) { /* if (pos > 0)*/
                Mi(prev_index, curr_index) += state.coef(f_index);
            }
            index++;
        }

        // take exponential operator
        for (size_t m = 0; m < state.num_labels; m++) {
            // update for Vi
            Vi(m) = std::exp(Vi(m));
            // update for Mi
            for (size_t n = 0; n < state.num_labels; n++) {
                Mi(m, n) = std::exp(Mi(m, n));
            }
        }
        temp=betas.row(i);
        temp=temp.cwiseProduct(Vi);
        betas.row(i -1) = Mi*temp; 
        // scale for the next (backward) beta values
        scale(i - 1)=betas.row(i-1).sum();
        betas.row(i - 1)*=(1.0 / scale(i - 1));
        index++;
    } // end of beta values computation
  
    index = 0;
    state.loglikelihood = 0;
    // start to compute the log-likelihood of the current sequence
    for (size_t j = 0; j < seq_len; j++) {
        Mi.fill(0);
        Vi.fill(0);
        // examine all features at position "pos"
        size_t ori_index = index;
        while (features(index)!=-1) {
            size_t f_index = features(index);
            size_t prev_index = prevLabel(index);
            size_t curr_index = currLabel(index);
            size_t f_type =  featureType(index);
            if (f_type == 0) {
                // state feature
                Vi(curr_index) += state.coef(f_index);
            } else if (f_type == 1) { /* if (pos > 0)*/
                Mi(prev_index, curr_index) += state.coef(f_index);
            }
            index++;
        }

        // take exponential operator
        for (size_t i = 0; i < state.num_labels; i++) {
            // update for Vi
            Vi(i) = std::exp(Vi(i));
            // update for Mi
            for (size_t j = 0; j < state.num_labels; j++) {
                Mi(i, j) = std::exp(Mi(i, j));
            }
        }

        if(j>0){
          temp = alpha;
          next_alpha=(temp.transpose()*Mi.transpose()).transpose();
          next_alpha=next_alpha.cwiseProduct(Vi);
        } else {
          next_alpha=Vi;
        }

        index = ori_index;
        while (features(index)!=-1) {
            size_t f_index = features(index);
            size_t prev_index = prevLabel(index);
            size_t curr_index = currLabel(index);
            size_t f_type =  featureType(index);
            if (f_type == 0 || f_type == 1) {
                state.grad(f_index) += 1;
                state.loglikelihood += state.coef(f_index);
            }
            if (f_type == 1) {
                ExpF(f_index) += next_alpha(curr_index) * 1 * betas(j,curr_index);
            } else if (f_type == 0) {
                ExpF(f_index) += alpha[prev_index] * Vi(curr_index) * Mi(prev_index,curr_index)
                                 * 1 * betas(j,curr_index);
            }
        }
        alpha = next_alpha;
        alpha*=(1.0 / scale[j]);
        index++;
    }

    // Zx = sum(alpha_i_n) where i = 1..num_labels, n = seq_len
    double Zx = alpha.sum();
    state.loglikelihood -= std::log(Zx);

    // re-correct the value of seq_logli because Zx was computed from
    // scaled alpha values
    for (size_t k = 0; k < seq_len; k++) {
        state.loglikelihood -= std::log(scale[k]);
    }
    // update the gradient vector
    for (size_t k = 0; k < state.num_features; k++) {
        state.grad_new(k) -= ExpF(k) / Zx;
    }

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
lincrf_cg_step_merge_states::run(AnyType &args) {
    LinCrfCGTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    LinCrfCGTransitionState<ArrayHandle<double> > stateRight = args[1];

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
lincrf_cg_step_final::run(AnyType &args) {
 // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    LinCrfCGTransitionState<MutableArrayHandle<double> > state = args[0];
    
    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

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
        
        // Alternatively, we could use Polak-Ribière
        // state.beta
        //     = dot(state.gradNew, gradNewMinusGrad)
        //     / dot(state.grad, state.grad);
        
        // Or Fletcher–Reeves
        // state.beta
        //     = dot(state.gradNew, state.gradNew)
        //     / dot(state.grad, state.grad);
        
        // Do a direction restart (Powell restart)
        // Note: This is testing whether state.beta < 0 if state.beta were
        // assigned according to Polak-Ribière
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
    
    state.iteration++;
    return state;
}

/**
 * @brief Return the difference in log-likelihood between two states
 */
AnyType
internal_lincrf_cg_step_distance::run(AnyType &args) {
    LinCrfCGTransitionState<ArrayHandle<double> > stateLeft = args[0];
    LinCrfCGTransitionState<ArrayHandle<double> > stateRight = args[1];
    return std::abs(stateLeft.loglikelihood - stateRight.loglikelihood);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_lincrf_cg_result::run(AnyType &args) {
    LinCrfCGTransitionState<ArrayHandle<double> > state = args[0];
    return stateToResult(*this, state.coef, state.loglikelihood);
}

/**
 * @brief Compute the diagnostic statistics
 *
 * This function wraps the common parts of computing the results for both the
 * CG and the IRLS method.
 */
AnyType stateToResult(
    const Allocator &inAllocator,
    const HandleMap<const ColumnVector, TransparentHandle<double> > &incoef,
    double loglikelihood) {
    // FIXME: We currently need to copy the coefficient to a native array
    // This should be transparent to user code
    HandleMap<ColumnVector> coef(
        inAllocator.allocateArray<double>(incoef.size()));
    coef = incoef;

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << loglikelihood << coef;
    return tuple;
}

} // namespace crf

} // namespace modules

} // namespace madlib
