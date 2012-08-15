/* ----------------------------------------------------------------------- *//**
 *
 * @file linear_crf.cpp
 *
 * @brief Linear-chain Conditional Random Field functions
 *
 * We implement limited-memory BFGS method.
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

        rebind(static_cast<uint32_t>(mStorage[1]));
    }

    inline operator AnyType() const {
        return mStorage;
    }

    inline void initialize(const Allocator &inAllocator, uint32_t inWidthOfX, uint32_t tagSize) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
        dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX));
        rebind(inWidthOfX);
        num_features = inWidthOfX;
        num_labels =  tagSize;
        if(iteration == 0)
            diag.fill(1);
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
        grad += inOtherState.grad;
        loglikelihood += inOtherState.loglikelihood;
        return *this;
    }

    inline void reset() {
        numRows = 0;
        grad.fill(0);
        loglikelihood = 0;
    }
    static const int m=3;
private:
    static inline uint32_t arraySize(const uint32_t num_features) {
        return 29 + 3 * num_features + num_features*(2*m+1)+2*m;
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
        loglikelihood.rebind(&mStorage[4 + 3 * inWidthOfFeature + inWidthOfFeature*(2*m+1)+2*m]);
        uint32_t model_size = 5 + 3 * inWidthOfFeature + inWidthOfFeature*(2*m+1)+2*m;
        //mcsrch states
        iflag.rebind(&mStorage[model_size + 0]);
        stp1.rebind(&mStorage[model_size + 1]);
        stp.rebind(&mStorage[model_size + 2]);
        iter.rebind(&mStorage[model_size + 3]);
        point.rebind(&mStorage[model_size + 4]);
        ispt.rebind(&mStorage[model_size + 5]);
        iypt.rebind(&mStorage[model_size + 6]);
        info.rebind(&mStorage[model_size + 7]);
        npt.rebind(&mStorage[model_size + 8]);
        uint32_t srch_size = model_size + 9;
        //mcstep states
        dgtest.rebind(&mStorage[srch_size + 0]);
        dginit.rebind(&mStorage[srch_size + 1]);
        dgx.rebind(&mStorage[srch_size + 2]);
        dgy.rebind(&mStorage[srch_size + 3]);
        finit.rebind(&mStorage[srch_size + 4]);
        fx.rebind(&mStorage[srch_size + 5]);
        fy.rebind(&mStorage[srch_size + 6]);
        stx.rebind(&mStorage[srch_size + 7]);
        sty.rebind(&mStorage[srch_size + 8]);
        brackt.rebind(&mStorage[srch_size + 9]);
        stage1.rebind(&mStorage[srch_size + 10]);
        stmin.rebind(&mStorage[srch_size + 11]);
        stmax.rebind(&mStorage[srch_size + 12]);
        width.rebind(&mStorage[srch_size + 13]);
        width1.rebind(&mStorage[srch_size + 14]);
    }
    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt32 iteration;
    typename HandleTraits<Handle>::ReferenceToUInt16 num_features;
    typename HandleTraits<Handle>::ReferenceToUInt16 num_labels;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap diag;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap grad;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap ws;

    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToDouble loglikelihood;

    typename HandleTraits<Handle>::ReferenceToUInt16 iflag;
    typename HandleTraits<Handle>::ReferenceToDouble stp1;
    typename HandleTraits<Handle>::ReferenceToDouble stp;
    typename HandleTraits<Handle>::ReferenceToUInt32 iter;
    typename HandleTraits<Handle>::ReferenceToUInt32 point;
    typename HandleTraits<Handle>::ReferenceToUInt32 ispt;
    typename HandleTraits<Handle>::ReferenceToUInt32 iypt;
    typename HandleTraits<Handle>::ReferenceToUInt32 info;
    typename HandleTraits<Handle>::ReferenceToUInt32 npt;

    typename HandleTraits<Handle>::ReferenceToDouble dgtest;
    typename HandleTraits<Handle>::ReferenceToDouble dginit;
    typename HandleTraits<Handle>::ReferenceToDouble dgx;
    typename HandleTraits<Handle>::ReferenceToDouble dgy;
    typename HandleTraits<Handle>::ReferenceToDouble finit;
    typename HandleTraits<Handle>::ReferenceToDouble fx;
    typename HandleTraits<Handle>::ReferenceToDouble fy;
    typename HandleTraits<Handle>::ReferenceToDouble stx;
    typename HandleTraits<Handle>::ReferenceToDouble sty;
    typename HandleTraits<Handle>::ReferenceToBool  brackt;
    typename HandleTraits<Handle>::ReferenceToDouble stage1;
    typename HandleTraits<Handle>::ReferenceToDouble stmin;
    typename HandleTraits<Handle>::ReferenceToDouble stmax;
    typename HandleTraits<Handle>::ReferenceToDouble width;
    typename HandleTraits<Handle>::ReferenceToDouble width1;
};

void mcstep(  LinCrfLBFGSTransitionState<MutableArrayHandle<double> >& T,
              double& stx, double& fx, double& dx,
              double& sty, double& fy, double& dy,
              double& stp, const double& fp, const double& dp,
              const double& stmin, const double& stmax, int& info)
{
    bool bound;
    double gamma, p, q, r, sgnd, stpc, stpf, stpq, theta, s;

    info = 0;

    if ((T.brackt && ((stp <= std::min(stx, sty)) || (stp >= std::max(stx, sty)))) ||
            (dx * (stp - stx) >= 0) || (stmax < stmin)) {
        return;
    }

    sgnd = dp*(dx/fabs(dx));
    if (fp > fx) {
        info = 1;
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
        T.brackt = true;

    } else if (sgnd < 0.0) {
        info = 2;
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
        T.brackt = true;

    } else if (fabs(dp) < fabs(dx)) {
        info = 3;
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
        if (T.brackt) {
            stpf = (fabs(stp - stpc) < fabs(stp - stpq)) ? stpc : stpq;
        } else {
            stpf = (fabs(stp - stpc) > fabs(stp - stpq)) ? stpc : stpq;
        }

    } else {
        info = 4;
        bound = false;
        if (T.brackt) {
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
    if (T.brackt && bound) {
        if (sty > stx) {
            stp = std::min(stx + 0.66*(sty - stx), stp);
        } else {
            stp = std::max(stx + 0.66*(sty - stx), stp);
        }
    }

    return;
}

void mcsrch( LinCrfLBFGSTransitionState<MutableArrayHandle<double> >& T)
{
    static const int MAXFEV = 20;
    int nfev = 0;
    static const double STPMIN = pow(10.0, -20.0);
    static const double STPMAX = pow(10.0, 20.0);
    static const double XTOL = 100.0 * std::numeric_limits<double>::min();
    static const double GTOL = 0.9;
    static const double FTOL = 0.0001;
    static const double XTRAPF = 4.0;
    const double f = T.loglikelihood;
    const int n = T.num_features;
    int infoc=0;
    if(T.info != -1) {
        infoc = 1;
        if (T.stp <= 0)  return ;

        T.dginit = T.grad.dot(T.ws.segment(T.ispt + T.point * n, n));
        if (T.dginit >= 0.0) {
            std::cout<<"The search direction is not a descent direction."<<std::endl;
            return;
        }

        T.brackt = false;
        T.stage1 = true;
        T.finit = f;
        T.dgtest = FTOL * T.dginit;
        T.width = STPMAX - STPMIN;
        T.width1 = 2.0 * T.width;

        T.diag = T.coef;

        T.stx = 0.0;
        T.fx = T.finit;
        T.dgx = T.dginit;
        T.sty = 0.0;
        T.fy = T.finit;
        T.dgy = T.dginit;
    }

    while(true)
    {
        if(T.info != -1)
        {
            if (T.brackt) {
                if (T.stx < T.sty) {
                    T.stmin = T.stx;
                    T.stmax = T.sty;
                } else {
                    T.stmin = T.sty;
                    T.stmax = T.stx;
                }
            } else {
                T.stmin = T.stx;
                T.stmax = T.stp + XTRAPF * (T.stp - T.stx);
            }

            if (T.stp > STPMAX) {
                T.stp = STPMAX;
            }
            if (T.stp < STPMIN) {
                T.stp = STPMIN;
            }
            if ((T.brackt && ((T.stp <= T.stmin) || (T.stp >= T.stmax))) || (nfev == MAXFEV - 1) ||
                    (!infoc) || (T.brackt && ((T.stmax - T.stmin) <= XTOL * T.stmax))) {
                T.stp = T.stx;
            }

            //std::cout<<"s:"<<s<<std::endl;
            T.coef = T.diag + T.stp * T.ws.segment(T.ispt + T.point * n, n);
            T.info = -1;
            return;
        }
        T.info = 0;
        //std::cout<<"x2:"<<x<<std::endl;
        //return false;
        double dg = T.grad.dot(T.ws.segment(T.ispt + T.point * n, n));
        double ftest1 = T.finit + T.stp * T.dgtest;

        if ((T.brackt && ((T.stp <= T.stmin) || (T.stp >= T.stmax))) || (!infoc)) {
            T.info = 6;
        }
        if ((T.stp == STPMAX) && (f <= ftest1) && (dg <= T.dgtest)) {
            T.info = 5;
        }
        if ((T.stp == STPMIN) && ((f >= ftest1) || (dg >= T.dgtest))) {
            T.info = 4;
        }
        if (nfev >= MAXFEV) {
            T.info = 3;
        }
        if (T.brackt && (T.stmax - T.stmin <= XTOL * T.stmax)) {
            T.info = 2;
        }
        if ((f <= ftest1) && (fabs(dg) <= -GTOL * T.dginit)) {
            T.info = 1;
        }
        if (T.info !=0 )
            return ;
        T.stage1 = T.stage1 && ((f > ftest1) || (dg < std::min(FTOL, GTOL) * T.dginit));

        if (T.stage1) {
            double fm = f - T.stp * T.dgtest;
            double fxm = T.fx - T.stx * T.dgtest;
            double fym = T.fy - T.sty * T.dgtest;
            double dgm = dg - T.dgtest;
            double dgxm = T.dgx - T.dgtest;
            double dgym = T.dgy - T.dgtest;
            //mcstep(T, T.stx, fxm, dgxm, T.sty, fym, dgym, T.stp, fm, dgm,T.stmin, T.stmax, infoc);
            T.fx = fxm + T.stx * T.dgtest;
            T.fy = fym + T.sty * T.dgtest;
            T.dgx = dgxm + T.dgtest;
            T.dgy = dgym + T.dgtest;
        } else {
            //mcstep(T, T.stx, T.fx, T.dgx, T.sty, T.fy, T.dgy, T.stp, f, dg, T.stmin, T.stmax, infoc);
        }

        if (T.brackt) {
            if (fabs(T.sty - T.stx) >= 0.66 * T.width1) {
                T.stp = T.stx + 0.5 * (T.sty - T.stx);
            }
            T.width1 = T.width;
            T.width = fabs(T.sty - T.stx);
        }
    }
}



void lbfgs(LinCrfLBFGSTransitionState<MutableArrayHandle<double> >& T, double eps)
{
    //local variables
    bool execute_entire_while_loop = false;
    const int n = T.num_features;
    const int m = T.m;
    int cp ;
    if(T.iflag == 0) {
        T.iter=0;
        cp=0;
        T.info=0;
        T.point = 0;
        T.ispt = n + 2*m;
        T.iypt = T.ispt + n*m;
        T.npt = 0;
        T.ws.segment(T.ispt, n) = (-T.grad).cwiseProduct(T.diag);
        T.stp1 = 1.0 / T.grad.norm();
        execute_entire_while_loop = true;
    }
    while(true) {
        if(execute_entire_while_loop) {
            T.iter++;
            T.info = 0;
            int bound = std::min(int(T.iter-1), m);
            if (T.iter!=1) {
                double ys = T.ws.segment(T.iypt + T.npt, n).dot(T.ws.segment(T.ispt + T.npt, n));
                double yy = T.ws.segment(T.iypt + T.npt, n).squaredNorm();
                T.diag.setConstant(ys / yy);
                cp = T.point;
                if (T.point ==0 ) cp =m;
                T.ws[n + cp-1] = 1.0 / ys;
                T.ws.head(n) = -T.grad;
                cp = T.point;
                for (int i = 0; i < bound; i++) {
                    cp -= 1;
                    if (cp == -1) {
                        cp = m - 1;
                    }
                    double sq = T.ws.segment(T.ispt + cp *n,n).dot(T.ws.head(n));
                    int inmc = n + m + cp;
                    int iycn = T.iypt + cp * n;
                    T.ws[inmc] = sq * T.ws[n + cp];
                    T.ws.head(n) -= T.ws[inmc] * T.ws.segment(iycn, n);
                }
                T.ws.head(n)=T.ws.head(n).cwiseProduct(T.diag);

                for (int i = 0; i < bound; i++) {
                    double yr = T.ws.segment(T.iypt + cp * n, n).dot(T.ws.head(n));
                    int inmc = n + m + cp;
                    int iscn = T.ispt + cp * n;
                    double beta = T.ws[inmc] - T.ws[n + cp] * yr;
                    T.ws.head(n) += beta * T.ws.segment(iscn, n);
                    cp += 1;
                    if (cp == m) {
                        cp = 0;
                    }
                }
                T.ws.segment(T.ispt + T.point * n, n) = T.ws.head(n);

            }
            T.stp = (T.iter == 1) ? T.stp1 : 1.0;
            T.ws.head(n) = T.grad;
        }
        mcsrch(T);
        if(T.info == -1) {
            T.iflag = 1;
            return;
        } else {
            std::cout<<"error"<<std::endl;
        }
        T.npt = T.point * n;
        T.ws.segment(T.ispt + T.npt,n) *=T.stp;
        T.ws.segment(T.iypt + T.npt,n) =T.grad - T.ws.head(n);
        T.point = T.point + 1;
        if (T.point == m) {
            T.point = 0;
        }
        if(T.grad.norm()/std::max(1.0,T.coef.norm())<=eps || T.iter>20) {
            T.iflag = 0;
            return ;
        }
        execute_entire_while_loop = true;
    }
}

void compute_log_Mi(int num_labels, Eigen::MatrixXd &Mi, Eigen::VectorXd &Vi) {
    // take exponential operator
    for (int m = 0; m < num_labels; m++) {
        Vi(m) = std::exp(Vi(m));
        for (int n = 0; n < num_labels; n++) {
            Mi(m, n) = std::exp(Mi(m, n));
        }
    }
}

AnyType
lincrf_lbfgs_step_transition::run(AnyType &args) {
    LinCrfLBFGSTransitionState<MutableArrayHandle<double> > state = args[0];
    HandleMap<const ColumnVector> features = args[1].getAs<ArrayHandle<double> >();
    int feature_size = features.size();
    int seq_len = features(feature_size-1) + 1;
    if (state.numRows == 0) {
        state.initialize(*this, args[2].getAs<double>(), args[3].getAs<double>());
        if (!args[4].isNull()) {
            LinCrfLBFGSTransitionState<ArrayHandle<double> > previousState = args[4];
            state = previousState;
            state.reset();
        }
    }

    Eigen::MatrixXd betas(state.num_labels, seq_len);
    Eigen::VectorXd scale(seq_len);
    Eigen::MatrixXd Mi(state.num_labels,state.num_labels);
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
    betas.col(seq_len - 1).fill(1.0 / scale(seq_len - 1));

    int index = feature_size-1;
    for (int i = seq_len - 1; i > 0; i--) {
        Mi.fill(0);
        Vi.fill(0);
        // examine all features at position "pos"
        while (index-5>=0 && features(index-1) == i) {
            size_t f_type =  features(index-5);
            size_t prev_index =  features(index-4);
            size_t curr_index =  features(index-3);
            size_t f_index =  features(index-2);
            if (f_type == 2) {// state feature
                Vi(curr_index) += state.coef(f_index);
            } else if (f_type == 1) { // edge feature
                Mi(prev_index, curr_index) += state.coef(f_index);
            }
            index-=6;
        }

        compute_log_Mi(state.num_labels, Mi, Vi);

        temp=betas.col(i);
        temp=temp.cwiseProduct(Vi);
        betas.col(i -1) = Mi*temp;
        // scale for the next (backward) beta values
        scale(i - 1)=betas.col(i-1).sum();
        betas.col(i - 1)*=(1.0 / scale(i - 1));
    } // end of beta values computation

    index = 0;
    state.loglikelihood = 0;
    // start to compute the log-likelihood of the current sequence
    for (int j = 0; j < seq_len; j++) {
        Mi.fill(0);
        Vi.fill(0);
        // examine all features at position "pos"
        size_t ori_index = index;
        while (((index+5) <= (feature_size-1)) && features(index+4) == j) {
            size_t f_type =  features(index);
            size_t prev_index =  features(index+1);
            size_t curr_index =  features(index+2);
            size_t f_index =  features(index+3);
            if (f_type == 2) {// state feature
                Vi(curr_index) += state.coef(f_index);
            } else if (f_type == 1) { //edge feature
                Mi(prev_index, curr_index) += state.coef(f_index);
            }
            index+=6;
        }

        compute_log_Mi(state.num_labels, Mi, Vi);

        if(j>0) {
            temp = alpha;
            next_alpha=Mi*temp;
            next_alpha=next_alpha.cwiseProduct(Vi);
        } else {
            next_alpha=Vi;
        }


        index = ori_index;
        while (((index+5) <= (feature_size-1)) && features(index+4) == j) {
            size_t f_type =  features(index);
            size_t prev_index =  features(index+1);
            size_t curr_index =  features(index+2);
            size_t f_index =  features(index+3);
            size_t exist = features(index+5);
            if (exist == 1) {
                state.grad(f_index) += 1;
                state.loglikelihood += state.coef(f_index);
            }
            if (f_type == 2) {
                ExpF(f_index) += next_alpha(curr_index) * betas(curr_index,j);
            } else if (f_type == 1) {
                ExpF(f_index) += alpha[prev_index] * Vi(curr_index) * Mi(prev_index,curr_index) * betas(curr_index, j);
            }
            index+=6;
        }
        alpha = next_alpha;
        alpha*=(1.0 / scale(j));
    }

    // Zx = sum(alpha_i_n) where i = 1..num_labels, n = seq_len
    double Zx = alpha.sum();
    state.loglikelihood -= std::log(Zx);

    // re-correct the value of seq_logli because Zx was computed from
    // scaled alpha values
    for (int k = 0; k < seq_len; k++) {
        state.loglikelihood -= std::log(scale(k));
    }
    // update the gradient vector
    for (size_t k = 0; k < state.num_features; k++) {
        state.grad(k) -= ExpF(k) / Zx;
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

    double sigma_square = 100;

    state.loglikelihood -= state.coef.dot(state.coef)/ 2 * sigma_square;
    state.loglikelihood *= -1;
    state.grad -= state.coef;
    state.grad = -state.grad;

    double eps = 1.0e-6;
    assert((state.m > 0) && (state.m <= state.num_features) && (eps >= 0.0));
    lbfgs(state, eps);
    if(state.iflag < 0)  throw std::logic_error("lbfgs failed");

    //throw std::logic_error("Internal error: Incompatible transition states");
    if(!state.coef.is_finite())
        //   throw NoSolutionFoundException("Over- or underflow in "
        //     "L-BFGS step, while updating coefficients. Input data "
        //    "is likely of poor numerical condition.");

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

    // Return all coefficients, loglikelihood, etc. in a tuple
    AnyType tuple;
    tuple << coef << loglikelihood;
    return tuple;
}

} // namespace crf

} // namespace modules

} // namespace madlib
