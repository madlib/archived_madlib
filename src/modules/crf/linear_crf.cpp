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
#include "linear_crf.hpp"

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
class LinCrfLBFGSTransitionState {
    template <class OtherHandle>
    friend class LinCrfLBFGSTransitionState;

public:
    LinCrfLBFGSTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[1]));
    }

    inline operator AnyType() const {
        return mStorage;
    }

    inline void initialize(const Allocator &inAllocator, uint16_t inWidthOfX, uint16_t tagSize) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
        dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(num_features));
        rebind(num_features);
        num_features = inWidthOfX;
        num_labels =  tagSize;
    }

    template <class OtherHandle>
    LinCrfLBFGSTransitionState &operator=(
        const LinCrfLBFGSTransitionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    template <class OtherHandle>
    LinCrfLBFGSTransitionState &operator+=(
        const LinCrfLBFGSTransitionState<OtherHandle> &inOtherState) {
        if (mStorage.size() != inOtherState.mStorage.size())
            throw std::logic_error("Internal error: Incompatible transition "
                                   "states");
        numRows += inOtherState.numRows;
        gradNew += inOtherState.gradNew;
        loglikelihood += inOtherState.loglikelihood;
        return *this;
    }

    inline void reset() {
        numRows = 0;
        gradNew.fill(0);
        loglikelihood = 0;
    }
    static const int m=3;
private:
    static inline uint64_t arraySize(const uint32_t num_features) {
        return 5 + 4 * num_features + num_features*(2*m+1)+2*m;
    }

    void rebind(uint32_t inWidthOfFeature) {
        iteration.rebind(&mStorage[0]);
        num_features.rebind(&mStorage[1]);
        num_labels.rebind(&mStorage[2]);
        coef.rebind(&mStorage[3], inWidthOfFeature);
        diag.rebind(&mStorage[3 + inWidthOfFeature], inWidthOfFeature);
        grad.rebind(&mStorage[3 + 2 * inWidthOfFeature], inWidthOfFeature);
        ws.rebind(&mStorage[3 + 3 * inWidthOfFeature], inWidthOfFeature*(2*m+1)+2*m);
        numRows.rebind(&mStorage[3 + 3 * inWidthOfFeature + inWidthOfFeature*(2*m+1)+2*m]);
        gradNew.rebind(&mStorage[4 + 3 * inWidthOfFeature + inWidthOfFeature*(2*m+1)+2*m], inWidthOfFeature);
        loglikelihood.rebind(&mStorage[4 + 4 * inWidthOfFeature + inWidthOfFeature*(2*m+1)+2*m]);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt32 iteration;
    typename HandleTraits<Handle>::ReferenceToUInt32 num_features;
    typename HandleTraits<Handle>::ReferenceToUInt16 num_labels;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap diag;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap grad;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap ws;

    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap gradNew;
    typename HandleTraits<Handle>::ReferenceToDouble loglikelihood;
};
// Protected Member Functions ------------------------------------------------
bool lbfgsStep(double& stx, double& fx, double& dx,
               double& sty, double& fy, double& dy,
               double& stp, const double& fp, const double& dp,
               bool& brackt, const double& stmin, const double& stmax)
{
    bool bound;
    double gamma;
    double p;
    double q;
    double r;
    double s;
    double stpc;
    double stpf;
    double stpq;
    double theta;

    if ((brackt && ((stp <= std::min(stx, sty)) || (stp >= std::max(stx, sty)))) ||
            (dx * (stp - stx) >= 0) || (stmax < stmin)) {
        return false;
    }

    double sgnd = dp*(dx/fabs(dx));
    if (fp > fx) {
        bound = true;
        theta = 3.0 * (fx - fp) / (stp - stx) + dx + dp;
        s = std::max(fabs(theta), std::max(fabs(dx), fabs(dp)));
        gamma = s * sqrt((theta / s) * (theta / s) - (dx / s) * (dp / s));
        if (stp < stx) {
            gamma = -gamma;
        }
        p = gamma - dx + theta;
        q = gamma - dx + gamma + dp;
        r = p / q;
        stpc = stx + r * (stp - stx);
        stpq = stx + dx/((fx - fp)/(stp - stx) + dx)/2 * (stp - stx);
        if (fabs(stpc - stx) < fabs(stpq - stx)) {
            stpf = stpc;
        } else {
            stpf = stpc + (stpq - stpc)/2;
        }
        brackt = true;

    } else {

        if (sgnd < 0.0) {
            bound = false;
            theta = 3.0 * (fx - fp) / (stp - stx) + dx + dp;
            s = std::max(fabs(theta), std::max(fabs(dx), fabs(dp)));
            gamma = s * sqrt((theta / s) * (theta / s) - (dx / s) * (dp / s));
            if (stp > stx) {
                gamma = -gamma;
            }
            p = gamma - dp + theta;
            q = gamma - dp + gamma + dx;
            r = p / q;
            stpc = stp + r * (stx - stp);
            stpq = stp + dp / (dp - dx) * (stx - stp);
            stpf = (fabs(stpc - stp) > fabs(stpq - stp)) ? stpc : stpq;
            brackt = true;

        } else {

            if (fabs(dp) < fabs(dx)) {
                bound = true;
                theta = 3.0 * (fx - fp) / (stp - stx) + dx + dp;
                s = std::max(fabs(theta), std::max(fabs(dx), fabs(dp)));
                gamma = s * sqrt(std::max(0.0, (theta/s)*(theta/s) - (dx/s)*(dp/s)));
                if (stp > stx) {
                    gamma = -gamma;
                }
                p = gamma - dp + theta;
                q = gamma + (dx - dp) + gamma;
                r = p / q;
                if ((r < 0.0) && (gamma != 0.0)) {
                    stpc = stp + r * (stx - stp);
                } else {
                    stpc = (stp > stx) ? stmax : stmin;
                }

                stpq = stp + dp / (dp - dx) * (stx - stp);
                if (brackt) {
                    stpf = (fabs(stp - stpc) < fabs(stp - stpq)) ? stpc : stpq;
                } else {
                    stpf = (fabs(stp - stpc) > fabs(stp - stpq)) ? stpc : stpq;
                }

            } else {
                bound = false;
                if (brackt) {
                    theta = 3.0 * (fp - fy) / (sty - stp) + dy + dp;
                    s = std::max(fabs(theta), std::max(fabs(dy), fabs(dp)));
                    gamma = s * sqrt((theta/s)*(theta/s) - (dy/s)*(dp/s));
                    if (stp > sty) {
                        gamma = -gamma;
                    }
                    p = gamma - dp + theta;
                    q = gamma - dp + gamma + dy;
                    r = p / q;
                    stpc = stp + r * (sty - stp);
                    stpf = stpc;
                } else {
                    stpf = (stp > stx) ? stmax : stmin;
                }
            }
        }
    }

    if (fp > fx) {
        sty = stp;
        fy = fp;
        dy = dp;
    } else {
        if (sgnd < 0.0) {
            sty = stx;
            fy = fx;
            dy = dx;
        }
        stx = stp;
        fx = fp;
        dx = dp;
    }

    stp = std::max(stmin, std::min(stmax, stpf));
    if (brackt && bound) {
        if (sty > stx) {
            stp = std::min(stx + 0.66*(sty - stx), stp);
        } else {
            stp = std::max(stx + 0.66*(sty - stx), stp);
        }
    }

    return true;
}

bool lbfgsSearch(double &f, const Eigen::VectorXd &s, double& stp, Eigen::VectorXd& diag, Eigen::VectorXd& x, Eigen::VectorXd& g)
{
    static const int MAXFEV = 20;
    static const double STPMIN = pow(10.0, -20.0);
    static const double STPMAX = pow(10.0, 20.0);
    static const double XTOL = 100.0 * std::numeric_limits<double>::min();
    static const double GTOL = 0.9;
    static const double FTOL = 0.0001;
    static const double XTRAPF = 4.0;

    if (stp <= 0) {
        return false;
    }

    double dginit = g.dot(s);
    if (dginit >= 0.0) {
        return false;
    }

    bool brackt = false;
    bool stage1 = true;
    double finit = f;
    double dgtest = FTOL * dginit;
    double width = STPMAX - STPMIN;
    double width1 = 2.0 * width;

    diag = x;

    double stx = 0.0;
    double fx = finit;
    double dgx = dginit;
    double sty = 0.0;
    double fy = finit;
    double dgy = dginit;
    bool infoc = true;


    double fm, fxm, fym;
    double dgm, dgxm, dgym;
    double stmin, stmax;

    for (int nfev = 0; nfev < MAXFEV; nfev++) {
        if (brackt) {
            if (stx < sty) {
                stmin = stx;
                stmax = sty;
            } else {
                stmin = sty;
                stmax = stx;
            }
        } else {
            stmin = stx;
            stmax = stp + XTRAPF * (stp - stx);
        }

        if (stp > STPMAX) {
            stp = STPMAX;
        }
        if (stp < STPMIN) {
            stp = STPMIN;
        }
        if ((brackt && ((stp <= stmin) || (stp >= stmax))) || (nfev == MAXFEV - 1) ||
                (!infoc) || (brackt && ((stmax - stmin) <= XTOL * stmax))) {
            stp = stx;
        }

        x = diag + stp * s;

        double dg = g.dot(s);
        double ftest1 = finit + stp * dgtest;

        if ((brackt && ((stp <= stmin) || (stp >= stmax))) || (!infoc)) {
            return true;
        }
        if ((stp == STPMAX) && (f <= ftest1) && (dg <= dgtest)) {
            return true;
        }
        if ((stp == STPMIN) && ((f >= ftest1) || (dg >= dgtest))) {
            return true;
        }
        if (brackt && (stmax - stmin <= XTOL * stmax)) {
            return true;
        }
        if ((f <= ftest1) && (fabs(dg) <= -GTOL * dginit)) {
            return true;
        }

        stage1 = stage1 && ((f > ftest1) || (dg < std::min(FTOL, GTOL) * dginit));

        if (stage1) {
            fm = f - stp * dgtest;
            fxm = fx - stx * dgtest;
            fym = fy - sty * dgtest;
            dgm = dg - dgtest;
            dgxm = dgx - dgtest;
            dgym = dgy - dgtest;
            infoc = lbfgsStep(stx, fxm, dgxm, sty, fym, dgym, stp, fm, dgm, brackt, stmin, stmax);
            fx = fxm + stx * dgtest;
            fy = fym + sty * dgtest;
            dgx = dgxm + dgtest;
            dgy = dgym + dgtest;
        } else {
            infoc = lbfgsStep(stx, fx, dgx, sty, fy, dgy, stp, f, dg, brackt, stmin, stmax);
        }

        if (brackt) {
            if (fabs(sty - stx) >= 0.66 * width1) {
                stp = stx + 0.5 * (sty - stx);
            }
            width1 = width;
            width = fabs(sty - stx);
        }
    }

    return true;
}



bool lbfgsMinimize(int m, int iter,
                   double epsg, double epsf, double epsx, unsigned  _n, Eigen::VectorXd x, double f,
                   Eigen::VectorXd g, Eigen::VectorXd diag, Eigen::VectorXd w)
{
    assert((m > 0) && (m <= (int)_n));
    assert((epsg >= 0.0) && (epsf >= 0.0) && (epsx >= 0.0));

    Eigen::VectorXd xold(_n);

    int point = 0;
    int ispt = _n + 2*m;
    int iypt = ispt + _n*m;
    int npt = 0;

    w.segment(ispt, _n) = -g;
    double stp1 = 1.0 / g.norm();

    xold = x;

    int bound = std::min(iter, m);
    if (iter != 0) {
        double ys = w.segment(iypt + npt, _n).dot(w.segment(ispt + npt, _n));
        double yy = w.segment(iypt + npt, _n).squaredNorm();
        diag.setConstant(ys / yy);
        w[_n + ((point == 0) ? m - 1 : point - 1)] = 1.0 / ys;
        w.head(_n) = -g;

        int cp = point;
        for (int i = 0; i < bound; i++) {
            cp -= 1;
            if (cp == -1) {
                cp = m - 1;
            }
            double sq = w.segment(ispt + cp * _n, _n).dot(w.head(_n));
            int inmc = _n + m + cp;
            int iycn = iypt + cp * _n;
            w[inmc] = sq * w[_n + cp];

            w.head(_n) -= w[inmc] * w.segment(iycn, _n);
        }

        w.head(_n) *= diag;

        for (int i = 0; i < bound; i++) {
            double yr = w.segment(iypt + cp * _n, _n).dot(w.head(_n));
            int inmc = _n + m + cp;
            int iscn = ispt + cp * _n;
            double beta = w[inmc] - w[_n + cp] * yr;

            w.head(_n) += beta * w.segment(iscn, _n);
            cp += 1;
            if (cp == m) {
                cp = 0;
            }
        }

        w.segment(ispt + point * _n, _n) = w.head(_n);
    }

    double stp = (iter == 0) ? stp1 : 1.0;
    w.head(_n) = g;

    bool success = lbfgsSearch(f, w.segment(ispt + point * _n, _n), stp, diag, x, g);
    if (!success) {
        return 0;
    }

    npt = point * _n;
    w.segment(ispt + npt, _n) *= stp;
    w.segment(iypt + npt, _n) = g - w.head(_n);
    point += 1;
    if (point == m) {
        point = 0;
    }

    return true;
}



AnyType
lincrf_lbfgs_step_transition::run(AnyType &args) {
    printf("Insert employee record - OK\n");
    throw std::domain_error("here1")
    LinCrfLBFGSTransitionState<MutableArrayHandle<double> > state = args[0];
    HandleMap<const ColumnVector> features = args[1].getAs<ArrayHandle<double> >();
    size_t feature_size = args[2].getAs<double>();
    size_t seq_len = features(features.size()-1) + 1;
    size_t tag_size = args[3].getAs<double>();
    if (state.numRows == 0) {
        state.initialize(*this, feature_size, tag_size);
        if (!args[4].isNull()) {
            LinCrfLBFGSTransitionState<ArrayHandle<double> > previousState = args[4];
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
        while (features(index+4) == i) {
            size_t f_type =  features(index);
            size_t prev_index =  features(index+1);
            size_t curr_index =  features(index+2);
            size_t f_index =  features(index+3);
            if (f_type == 2) {
                // state feature
                Vi(curr_index) += state.coef(f_index);
            } else if (f_type == 1) { /* if (pos > 0)*/
                Mi(prev_index, curr_index) += state.coef(f_index);
            }
            index+=4;
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
    } // end of beta values computation
  
    index = 0;
    state.loglikelihood = 0;
    // start to compute the log-likelihood of the current sequence
    for (size_t j = 0; j < seq_len; j++) {
        Mi.fill(0);
        Vi.fill(0);
        // examine all features at position "pos"
        while (features(index+4) == j) {
            size_t f_type =  features(index);
            size_t prev_index =  features(index+1);
            size_t curr_index =  features(index+2);
            size_t f_index =  features(index+3);
            if (f_type == 2) {
                // state feature
                Vi(curr_index) += state.coef(f_index);
            } else if (f_type == 1) { /* if (pos > 0)*/
                Mi(prev_index, curr_index) += state.coef(f_index);
            }
            index+=4;
        }
        size_t ori_index = index;

        // take exponential operator
        
        //Mi.noalias() = Mi.exp();// update for Mi
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
            size_t f_type =  features(index);
            size_t prev_index =  features(index+1);
            size_t curr_index =  features(index+2);
            size_t f_index =  features(index+3);
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
        state.gradNew(k) -= ExpF(k) / Zx;
    }

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
lincrf_lbfgs_step_merge_states::run(AnyType &args) {
    LinCrfLBFGSTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    LinCrfLBFGSTransitionState<ArrayHandle<double> > stateRight = args[1];

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
lincrf_lbfgs_step_final::run(AnyType &args) {
 // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    LinCrfLBFGSTransitionState<MutableArrayHandle<double> > state = args[0];
    
    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    // Note: k = state.iteration
    if (state.iteration == 0) {
		// Iteration computes the gradient
		state.grad = state.gradNew; 
    } else {
       //Eigen::VectorXd coef(state.num_features);
       //Eigen::VectorXd grad(state.num_features);
       //Eigen::VectorXd diag(state.num_features);
       //Eigen::VectorXd ws(state.num_features);
       lbfgsMinimize(3, state.iteration,
                   0.001, 0.001, 0.001, state.num_features, 
                   state.coef, state.loglikelihood,
                   state.grad, state.diag, state.ws);
    }    
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
internal_lincrf_lbfgs_step_distance::run(AnyType &args) {
    LinCrfLBFGSTransitionState<ArrayHandle<double> > stateLeft = args[0];
    LinCrfLBFGSTransitionState<ArrayHandle<double> > stateRight = args[1];
    return std::abs(stateLeft.loglikelihood - stateRight.loglikelihood);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_lincrf_lbfgs_result::run(AnyType &args) {
    LinCrfLBFGSTransitionState<ArrayHandle<double> > state = args[0];
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
