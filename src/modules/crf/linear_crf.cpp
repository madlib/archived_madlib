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

class LBFGS {
    //shared variables in the lbfgs
public:
    static  const double gtol = 0.9;
    public static double stpmin = 1e-20;
    public static double stpmax = 1e20;
    public double gnorm = 0, stp1 = 0, ftol = 0, stp = 0, ys = 0, yy = 0, sq = 0, yr = 0, beta = 0, xnorm = 0;
    public int iter = 0, nfun = 0, point = 0, ispt = 0, iypt = 0, maxfev = 0, info=0, bound = 0, npt = 0, cp = 0, i = 0, nfev = 0, inmc = 0, iycn = 0, iscn = 0;
    //shared varibles in the mcscrch
    public int infoc = 0, j = 0;
    public double dg = 0, dgm = 0, dginit = 0, dgtest = 0, dgx = 0, dgxm = 0, dgy = 0, dgym[] = 0, finit = 0, ftest1 = 0, fm = 0, fx = 0, fxm = 0, fy = 0, fym = 0, p5 = 0, p66 = 0, stx = 0, sty = 0, stmin = 0, stmax = 0, width = 0, width1 = 0, xtrapf = 0;
    public bool brackt = false, stage1 = false;

    public void mcstep ( double&, double& , double&, double&, double& , double&, double&, double fp , double dp, bool brackt, double, double, int&);

    public void mcsrch ( int, double&, double f , Eigen::VectorXd& g , Eigen::VectorXd& s , int is0 , double&, double ftol , double xtol, int maxfev , int&, int&, Eigen::VectorXd& wa );
};

void LBFGS::mcstep(double& stx, double& fx, double& dx,
                   double& sty, double& fy, double& dy,
                   double& stp, const double& fp, const double& dp, bool& brackt,
                   const double& stmin, const double& stmax, int& info)
{
    bool bound;
    double gamma, p, q, r, sgnd, stpc, stpf, stpq, theta, s;

    info = 0;

    if ((brackt && ((stp <= std::min(stx, sty)) || (stp >= std::max(stx, sty)))) ||
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
        brackt = true;

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
        brackt = true;

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
        if (brackt) {
            stpf = (fabs(stp - stpc) < fabs(stp - stpq)) ? stpc : stpq;
        } else {
            stpf = (fabs(stp - stpc) > fabs(stp - stpq)) ? stpc : stpq;
        }

    } else {
        info = 4;
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

void LBFGS::mcsrch(int n , Eigen::VectorXd& x,, double f , Eigen::VectorXd& g , Eigen::VectorXd& s , int is0 , double[] stp , double ftol , double xtol , int maxfev , int& info , int& nfev , Eigen::VectorXd& wa)
{
    p5 = 0.5;
    p66 = 0.66;
    xtrapf = 4.0;
    if(info != -1) {
        infoc = 1;
        if (n <= 0 || stp[0] <= 0 || ftol < 0 || LBFGS.gtol < 0 || xtol < 0 || LBFGS.stpmin < 0 || LBFGS.stpmax < LBFGS.stpmin || maxfev <= 0 )
            return;

        dginit = grad.dot(ws.segment(ispt + point * n, n));
        if (dginit >= 0.0) {
            std::cout<<"The search direction is not a descent direction."<<std::endl;
            return;
        }

        brackt = false;
        stage1 = true;
        nfev = 0;
        finit = f;
        dgtest = ftol * dginit;
        width = LBFGS.stpmax - LBFGS.stpmin;
        width1 = width/p5;

        wa = x;

        stx = 0.0;
        fx = finit;
        dgx = dginit;
        sty = 0.0;
        fy = finit;
        dgy = dginit;
    }

    while(true)
    {
        if(info != -1)
        {
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
                stmax = stp + xtrapf * (stp - stx);
            }

            if (stp > LBFGS.stpmax) {
                stp = LBFGS.stpmax;
            }
            if (stp < LBFGS.stpmin) {
                stp = LBFGS.stpmin;
            }
            if ((brackt && ((stp <= stmin) || (stp >= stmax))) || (nfev == maxfev - 1) ||
                    (!infoc) || (brackt && ((stmax - stmin) <= xtol * stmax))) {
                stp = stx;
            }

            x = wa + stp * s;
            info = -1;
            return;
        }
        info = 0;
        nfev= nfev + 1;
        //std::cout<<"x2:"<<x<<std::endl;
        //return false;
        dg = grad.dot(s);
        ftest1 = finit + stp * dgtest;

        if ((brackt && ((stp <= stmin) || (stp >= stmax))) || (!infoc)) {
            info = 6;
        }
        if ((stp == LBFGS.stpmax) && (f <= ftest1) && (dg <= dgtest)) {
            info = 5;
        }
        if ((stp == LBFGS.stpmin) && ((f >= ftest1) || (dg >= dgtest))) {
            info = 4;
        }
        if (nfev >= maxfev) {
            info = 3;
        }
        if (brackt && (stmax - stmin <= xtol * stmax)) {
            info = 2;
        }
        if ((f <= ftest1) && (fabs(dg) <= -LBFGS.gtol * dginit)) {
            info = 1;
        }
        if (info !=0 )
            return ;


        if ( stage1 && f <= ftest1 && dg >= std::min(ftol , LBFGS.gtol) * dginit ) stage1 = false;

        if (stage1 && f <= fx[0] && f > ftest1) {
            fm = f - stp * dgtest;
            fxm = fx - stx * dgtest;
            fym = fy - sty * dgtest;
            dgm = dg - dgtest;
            dgxm = dgx - dgtest;
            dgym = dgy - dgtest;
            mcstep(stx, fxm, dgxm, sty, fym, dgym, stp, fm, dgm, brackt, stmin, stmax, infoc);
            fx = fxm + stx * dgtest;
            fy = fym + sty * dgtest;
            dgx = dgxm + dgtest;
            dgy = dgym + dgtest;
        } else {
            mcstep(stx, fx, dgx, sty, fy, dgy, stp, f, dg, brackt, stmin, stmax, infoc);
        }

        if (brackt) {
            if (fabs(sty - stx) >= p66 * width1) {
                stp = stx + p5 * (sty - stx);
            }
            width1 = width;
            width = fabs(sty - stx);
        }
    }
}



void LBFGS::lbfgs(int n, int m, Eigen::VectorXd x, double f, Eigen::VectorXd g, bool diagco, double[] diag, double eps , double xtol , int[] iflag)
{
    bool execute_entire_while_loop = false;
    if(iflag == 0) {
        iter=0;
        if ( n <= 0 || m <= 0 )
        {
            iflag[0]= -3;
        }

        if ( gtol <= 0.0001 )
        {
            gtol= 0.9;
        }

        nfun= 1;
        point = 0;
        finish = false;
        ispt = n + 2*m;
        iypt = ispt + n*m;
        npt = 0;
        ws.segment(ispt, n) = (-grad).cwiseProduct(diag);
        stp1 = 1.0 / grad.norm();
        ftol= 0.0001;
        maxfev= 20;
        execute_entire_while_loop = true;
    }
    while(true) {
        if(execute_entire_while_loop) {
            iter++;
            info = 0;
            bound=iter-1;
            if (iter!=1) {
                if (iter > m) bound = m;
                ys = ws.segment(iypt + npt, n).dot(ws.segment(ispt + npt, n));
                yy = ws.segment(iypt + npt, n).squaredNorm();
                diag.setConstant(ys / yy);
                cp = point;
                if (point ==0 ) cp =m;
                ws[n + cp-1] = 1.0 / ys;
                ws.head(n) = -grad;
                cp = point;
                for (int i = 0; i < bound; i++) {
                    cp -= 1;
                    if (cp == -1) {
                        cp = m - 1;
                    }
                    sq = ws.segment(ispt + cp *n,n).dot(ws.head(n));
                    inmc = n + m + cp;
                    iycn = iypt + cp * n;
                    w[inmc] = sq * w[n + cp];
                    w.head(n) -= w[inmc] * w.segment(iycn, n);
                }
                w.head(n)=w.head(n).cwiseProduct(diag);

                for (int i = 0; i < bound; i++) {
                    yr = ws.segment(iypt + cp * n, n).dot(ws.head(n));
                    inmc = n + m + cp;
                    beta = w[inmc] - w[n + cp] * yr;
                    iscn = ispt + cp * n;
                    ws.head(n) += beta * ws.segment(iscn, n);
                    cp += 1;
                    if (cp == m) {
                        cp = 0;
                    }
                }
                ws.segment(ispt + point * n, n) = ws.head(n);
            }
            nfev = 0;
            stp = (iter == 1) ? stp1 : 1.0;
            ws.head(n) = grad;
        }
        mcsrch(n, x, f, g, w, ispt + point * n, stp, ftol, xtol, maxfev, info, nfev, diag);
        if(info == -1) {
            iflag = 1;
            return;
        } else {
            iflag = -1;
        }
        nfun = nfun + nfev;
        npt = point * n;
        ws.segment(ispt + npt,n) *=stp;
        ws.segment(iypt + npt,n) =grad - ws.head(n);
        point = point + 1;
        if (point == m) {
            point = 0;
        }
        if(grad.norm()/std::max(1.0,coef.norm())<=eps) {
            finish = true;
        }
        if (finish) {
            iflag = 0;
            return;
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
