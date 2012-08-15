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

    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap lbfgs_state;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap mcsrch_state;
};

class LBFGS {
    //shared variables in the lbfgs
public:
    //double gtol = 0.9;
    static double gtol;
    //double stpmin = 1e-20;
    static double stpmin;
    //double stpmax = 1e20
    static double stpmax;
    //shared variable in lbfgs
    double stp1, ftol, stp, ys, yy, sq, yr, beta;
    int iter, nfun, point, ispt, iypt, maxfev, info, bound, npt, cp, nfev, inmc, iycn, iscn;
    //shared varibles in mcscrch
    int infoc;
    double dg, dgm, dginit, dgtest, dgx, dgxm, dgy, dgym, finit, ftest1, fm, fx, fxm, fy, fym, p5, p66, stx, sty, stmin, stmax, width, width1, xtrapf;
    bool brackt, stage1, finish;

    LBFGS(LinCrfLBFGSTransitionState<MutableArrayHandle<double> >);
    void LBFGS::save_state(LinCrfLBFGSTransitionState<MutableArrayHandle<double> > state){
    void mcstep (double&, double& , double&, double&, double& , double&, double&, double, double, bool&, double, double, int&);
    void mcsrch (int, Eigen::VectorXd&, double, Eigen::VectorXd&, Eigen::VectorXd&, double&, double, double, int, int&, int&, Eigen::VectorXd&);
    void lbfgs(int, int, Eigen::VectorXd&, double, Eigen::VectorXd, Eigen::VectorXd&, double, double, int&);
};

double LBFGS::gtol = 0.9; 
double LBFGS::stpmin = 1e-20; 
double LBFGS::stpmax = 1e20; 

void LBFGS::LBFGS(LinCrfLBFGSTransitionState<MutableArrayHandle<double> > state){
    stp1=state.lbfgs_state(0); 
    ftol=state.lbfgs_state(0);
    stp=state.lbfgs_state(0);
    ys=state.lbfgs_state(0);
    yy=state.lbfgs_state(0);
    sq=state.lbfgs_state(0);
    yr=state.lbfgs_state(0);
    beta=state.lbfgs_state(0);
    iter=state.lbfgs_state(0);
    nfun=state.lbfgs_state(0);
    point=state.lbfgs_state(0);
    ispt=state.lbfgs_state(0);
    iypt=state.lbfgs_state(0);
    maxfev=state.lbfgs_state(0);
    info=state.lbfgs_state(0);
    bound=state.lbfgs_state(0);
    npt=state.lbfgs_state(0);
    cp=state.lbfgs_state(0);
    nfev=state.lbfgs_state(0);
    inmc=state.lbfgs_state(0);
    iycn=state.lbfgs_state(0);
    iscn=state.lbfgs_state(0);

    infoc=state.mcsrch_sate(0);
    dg=state.mcsrch_sate(0);
    dgm=state.mcsrch_sate(0);
    dginit=state.mcsrch_sate(0);
    dgtest=state.mcsrch_sate(0);
    dgx=state.mcsrch_sate(0);
    dgxm=state.mcsrch_sate(0);
    dgy=state.mcsrch_sate(0);
    dgym=state.mcsrch_sate(0);
    finit=state.mcsrch_sate(0);
    fest1=state.mcsrch_sate(0);
    fm=state.mcsrch_sate(0);
    fx=state.mcsrch_sate(0);
    fxm=state.mcsrch_sate(0);
    fy=state.mcsrch_sate(0);
    fym=state.mcsrch_sate(0);
    p5=state.mcsrch_sate(0);
    p66=state.mcsrch_sate(0);
    stx=state.mcsrch_sate(0);
    sty=state.mcsrch_sate(0);
    stmin=state.mcsrch_sate(0);
    stmax=state.mcsrch_sate(0);
    width=state.mcsrch_sate(0);
    width1=state.mcsrch_sate(0);
    xrapf=state.mcsrch_sate(0);
    bracket=state.mcsrch_sate(0);
    stage1=state.mcsrch_sate(0);
    finish=state.mcsrch_sate(0);
}
void LBFGS::save_state(LinCrfLBFGSTransitionState<MutableArrayHandle<double> > state){
}
void LBFGS::mcstep(double& stx, double& fx, double& dx,
                   double& sty, double& fy, double& dy,
                   double& stp, double fp, double dp, bool& brackt,
                   double stmin, double stmax, int& info)
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
    if (brackt && bound) {
        if (sty > stx) {
            stp = std::min(stx + 0.66*(sty - stx), stp);
        } else {
            stp = std::max(stx + 0.66*(sty - stx), stp);
        }
    }

    return;
}

void LBFGS::mcsrch(int n, Eigen::VectorXd& x, double f, Eigen::VectorXd& g, Eigen::VectorXd& s, double& stp, double ftol, double xtol, int maxfev, int& info, int& nfev, Eigen::VectorXd& wa)
{
    p5 = 0.5;
    p66 = 0.66;
    xtrapf = 4.0;
    if(info != -1) {
        infoc = 1;
        if (n <= 0 || stp <= 0 || ftol < 0 || gtol < 0 || xtol < 0 || stpmin < 0 || stpmax < stpmin || maxfev <= 0 )
            return;

        dginit = g.dot(s);
        if (dginit >= 0.0) {
            std::cout<<"The search direction is not a descent direction."<<std::endl;
            return;
        }

        brackt = false;
        stage1 = true;
        nfev = 0;
        finit = f;
        dgtest = ftol * dginit;
        width = stpmax - stpmin;
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

            if (stp > stpmax) {
                stp = stpmax;
            }
            if (stp < stpmin) {
                stp = stpmin;
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
        dg = g.dot(s);
        ftest1 = finit + stp * dgtest;

        if ((brackt && ((stp <= stmin) || (stp >= stmax))) || (!infoc)) {
            info = 6;
        }
        if ((stp == stpmax) && (f <= ftest1) && (dg <= dgtest)) {
            info = 5;
        }
        if ((stp == stpmin) && ((f >= ftest1) || (dg >= dgtest))) {
            info = 4;
        }
        if (nfev >= maxfev) {
            info = 3;
        }
        if (brackt && (stmax - stmin <= xtol * stmax)) {
            info = 2;
        }
        if ((f <= ftest1) && (fabs(dg) <= -gtol * dginit)) {
            info = 1;
        }
        if (info !=0 )
            return ;


        if ( stage1 && f <= ftest1 && dg >= std::min(ftol , gtol) * dginit ) stage1 = false;

        if (stage1 && f <= fx && f > ftest1) {
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



void LBFGS::lbfgs(int n, int m, Eigen::VectorXd& x, double f, Eigen::VectorXd g, Eigen::VectorXd& diag, double eps , double xtol , int& iflag)
{
    bool execute_entire_while_loop = false;
    if(iflag == 0) {
        iter=0;
        if ( n <= 0 || m <= 0 )
        {
            iflag= -3;
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
        w.segment(ispt, n) = (-g).cwiseProduct(diag);
        stp1 = 1.0 / g.norm();
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
                ys = w.segment(iypt + npt, n).dot(w.segment(ispt + npt, n));
                yy = w.segment(iypt + npt, n).squaredNorm();
                diag.setConstant(ys / yy);
                cp = point;
                if (point ==0 ) cp =m;
                w[n + cp-1] = 1.0 / ys;
                w.head(n) = -g;
                cp = point;
                for (int i = 0; i < bound; i++) {
                    cp -= 1;
                    if (cp == -1) {
                        cp = m - 1;
                    }
                    sq = w.segment(ispt + cp *n,n).dot(w.head(n));
                    inmc = n + m + cp;
                    iycn = iypt + cp * n;
                    w[inmc] = sq * w[n + cp];
                    w.head(n) -= w[inmc] * w.segment(iycn, n);
                }
                w.head(n)=w.head(n).cwiseProduct(diag);

                for (int i = 0; i < bound; i++) {
                    yr = w.segment(iypt + cp * n, n).dot(w.head(n));
                    inmc = n + m + cp;
                    beta = w[inmc] - w[n + cp] * yr;
                    iscn = ispt + cp * n;
                    w.head(n) += beta * w.segment(iscn, n);
                    cp += 1;
                    if (cp == m) {
                        cp = 0;
                    }
                }
                w.segment(ispt + point * n, n) = w.head(n);
            }
            nfev = 0;
            stp = (iter == 1) ? stp1 : 1.0;
            w.head(n) = g;
        }
        mcsrch(n, x, f, g, w.segment(ispt + point * n, n), stp, ftol, xtol, maxfev, info, nfev, diag);
        if(info == -1) {
            iflag = 1;
            return;
        } else {
            iflag = -1;
        }
        nfun = nfun + nfev;
        npt = point * n;
        w.segment(ispt + npt,n) *=stp;
        w.segment(iypt + npt,n) =g - w.head(n);
        point = point + 1;
        if (point == m) {
            point = 0;
        }
        if(g.norm()/std::max(1.0,x.norm())<=eps) {
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
    double xtol = 1.0e-16;

    assert((state.m > 0) && (state.m <= state.num_features) && (eps >= 0.0));
   
    LBFGS instance = new lbfgs(state);
    instance.lbfgs(state.num_features, state.m, instance.x, state.loglikehood, state.grad, instance.diag, eps, xtol, instance.iflag)
    instance.save_state(state); 

    if(state.iflag < 0)  throw std::logic_error("lbfgs failed");

    if(!state.coef.is_finite())
      throw NoSolutionFoundException("Over- or underflow in "
         "L-BFGS step, while updating coefficients. Input data "
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

    // Return all coefficients, loglikelihood, etc. in a tuple
    AnyType tuple;
    tuple << coef << loglikelihood;
    return tuple;
}

} // namespace crf

} // namespace modules

} // namespace madlib
