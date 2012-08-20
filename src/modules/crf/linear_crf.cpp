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

/**
* @brief Inter- and intra-iteration state for lbfgs method for
* linear-chain conditional random field
*
* TransitionState encapsualtes the transition state during the
* linear-chain crf aggregate function. To the database, the state is
* exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
* object containing scalars and vectors.
*
* Note: We assume that the DOUBLE PRECISION array is initialized by the
* database with length at least 58, and all elemenets are 0.
*
*/
template <class Handle>
class LinCrfLBFGSTransitionState {
    template <class OtherHandle>
    friend class LinCrfLBFGSTransitionState;

public:
    LinCrfLBFGSTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint32_t>(mStorage[1]));
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
     * @brief Initialize the lbfgs state.
     *
     * This function is only called for the first row.
     */
    inline void initialize(const Allocator &inAllocator, uint32_t inWidthOfX, uint32_t tagSize) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
        dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX));
        rebind(inWidthOfX);
        num_features = inWidthOfX;
        num_labels =  tagSize;
        if(iteration == 0)
            diag.fill(1);
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    LinCrfLBFGSTransitionState &operator=(
        const LinCrfLBFGSTransitionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     * fields
     */
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

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        grad.fill(0);
        loglikelihood = 0;
    }

    static const int m=3;

private:
    static inline uint32_t arraySize(const uint32_t num_features) {
        return 52 + 3 * num_features + num_features*(2*m+1)+2*m;
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
        lbfgs_state.rebind(&mStorage[5 + 3 * inWidthOfFeature + inWidthOfFeature*(2*m+1)+2*m], 21);
        mcsrch_state.rebind(&mStorage[26 + 3 * inWidthOfFeature + inWidthOfFeature*(2*m+1)+2*m], 25);
    }
    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt32 iteration;
    typename HandleTraits<Handle>::ReferenceToUInt32 num_features;
    typename HandleTraits<Handle>::ReferenceToUInt32 num_labels;
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
    //shared variable in lbfgs
    double stp1, ftol, stp, sq, yr, beta;
    int  iflag, iter, nfun, point, ispt, iypt, maxfev, info, bound, npt, cp, nfev, inmc, iycn, iscn;
    //shared varibles in mcscrch
    int infoc;
    double dg, dgm, dginit, dgtest, dgx, dgxm, dgy, dgym, finit, ftest1, fm, fx, fxm, fy, fym, p5, p66, stx, sty, stmin, stmax, width, width1, xtrapf;
    bool brackt, stage1, finish;

    ColumnVector w;
    ColumnVector x;
    ColumnVector diag;

    LBFGS(LinCrfLBFGSTransitionState<MutableArrayHandle<double> >&);
    void save_state(LinCrfLBFGSTransitionState<MutableArrayHandle<double> > &state);
    void mcstep (double&, double& , double&, double&, double& , double&, double&, double, double, bool&, double, double, int&);
    void mcsrch (int, Eigen::VectorXd&, double, Eigen::VectorXd&, const Eigen::VectorXd&, double&, double, double, int, int&, int&, Eigen::VectorXd&);
    void lbfgs(int, int, double, Eigen::VectorXd, double, double);
};

LBFGS::LBFGS(LinCrfLBFGSTransitionState<MutableArrayHandle<double> >& state) {
    w = state.ws;
    diag = state.diag;
    x = state.coef;

    stp1 = state.lbfgs_state(0);
    ftol = state.lbfgs_state(1);
    stp = state.lbfgs_state(2);
    sq = state.lbfgs_state(3);
    yr = state.lbfgs_state(4);
    beta = state.lbfgs_state(5);
    iflag = static_cast<int>(state.lbfgs_state(6));
    iter = static_cast<int>(state.lbfgs_state(7));
    nfun = static_cast<int>(state.lbfgs_state(8));
    point = static_cast<int>(state.lbfgs_state(9));
    ispt = static_cast<int>(state.lbfgs_state(10));
    iypt = static_cast<int>(state.lbfgs_state(11));
    maxfev = static_cast<int>(state.lbfgs_state(12));
    info = static_cast<int>(state.lbfgs_state(13));
    bound = static_cast<int>(state.lbfgs_state(14));
    npt = static_cast<int>(state.lbfgs_state(15));
    cp = static_cast<int>(state.lbfgs_state(16));
    nfev = static_cast<int>(state.lbfgs_state(17));
    inmc = static_cast<int>(state.lbfgs_state(18));
    iycn = static_cast<int>(state.lbfgs_state(19));
    iscn = static_cast<int>(state.lbfgs_state(20));

    infoc = static_cast<int>(state.mcsrch_state(0));
    dg = state.mcsrch_state(1);
    dgm = state.mcsrch_state(2);
    dginit = state.mcsrch_state(3);
    dgtest = state.mcsrch_state(4);
    dgx = state.mcsrch_state(5);
    dgxm = state.mcsrch_state(6);
    dgy = state.mcsrch_state(7);
    dgym = state.mcsrch_state(8);
    finit = state.mcsrch_state(9);
    ftest1 = state.mcsrch_state(10);
    fm = state.mcsrch_state(11);
    fx = state.mcsrch_state(12);
    fxm = state.mcsrch_state(13);
    fy = state.mcsrch_state(14);
    fym = state.mcsrch_state(15);
    stx = state.mcsrch_state(16);
    sty = state.mcsrch_state(17);
    stmin = state.mcsrch_state(18);
    stmax = state.mcsrch_state(19);
    width = state.mcsrch_state(20);
    width1 = state.mcsrch_state(21);
    brackt = (state.mcsrch_state(22) == 1.0 ? true: false);
    stage1 = (state.mcsrch_state(23) == 1.0 ? true: false);
    finish = (state.mcsrch_state(24) == 1.0 ? true: false);
}
void LBFGS::save_state(LinCrfLBFGSTransitionState<MutableArrayHandle<double> > &state) {
    state.ws = w ;
    state.diag = diag ;
    state.coef = x ;

    state.lbfgs_state(0) = stp1;
    state.lbfgs_state(1) = ftol;
    state.lbfgs_state(2) = stp;
    state.lbfgs_state(3) = sq;
    state.lbfgs_state(4) = yr;
    state.lbfgs_state(5) = beta;
    state.lbfgs_state(6) = iflag;
    state.lbfgs_state(7) = iter;
    state.lbfgs_state(8) = nfun;
    state.lbfgs_state(9) = point;
    state.lbfgs_state(10) = ispt;
    state.lbfgs_state(11) = iypt;
    state.lbfgs_state(12) = maxfev;
    state.lbfgs_state(13) = info;
    state.lbfgs_state(14) = bound;
    state.lbfgs_state(15) = npt;
    state.lbfgs_state(16) = cp;
    state.lbfgs_state(17) = nfev;
    state.lbfgs_state(18) = inmc;
    state.lbfgs_state(19) = iycn;
    state.lbfgs_state(20) = iscn;

    state.mcsrch_state(0) = infoc;
    state.mcsrch_state(1) =  dg;
    state.mcsrch_state(2) = dgm;
    state.mcsrch_state(3) = dginit;
    state.mcsrch_state(4) = dgtest;
    state.mcsrch_state(5) = dgx;
    state.mcsrch_state(6) = dgxm;
    state.mcsrch_state(7) = dgy;
    state.mcsrch_state(8) = dgym;
    state.mcsrch_state(9) = finit;
    state.mcsrch_state(10) = ftest1;
    state.mcsrch_state(11) = fm;
    state.mcsrch_state(12) = fx;
    state.mcsrch_state(13) = fxm;
    state.mcsrch_state(14) = fy;
    state.mcsrch_state(15) = fym;
    state.mcsrch_state(16) = stx;
    state.mcsrch_state(17) = sty;
    state.mcsrch_state(18) = stmin;
    state.mcsrch_state(19) = stmax;
    state.mcsrch_state(20) = width;
    state.mcsrch_state(21) = width1;
    state.mcsrch_state(22) = (brackt == true ? 1.0 : 0.0);
    state.mcsrch_state(23) = (stage1 == true ? 1.0 : 0.0);
    state.mcsrch_state(24) = (finish == true ? 1.0 : 0.0);
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

void LBFGS::mcsrch(int n, Eigen::VectorXd& x, double f, Eigen::VectorXd& g, const Eigen::VectorXd& s, double& stp, double ftol, double xtol, int maxfev, int& info, int& nfev, Eigen::VectorXd& wa)
{
    double stpmin = 1e-20;
    double stpmax = 1e20;
    double p5 = 0.5;
    double p66 = 0.66;
    double xtrapf = 4.0;
    double gtol = 0.9;

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



void LBFGS::lbfgs(int n, int m, double f, Eigen::VectorXd g, double eps , double xtol)
{
    bool execute_entire_while_loop = false;
    if(iflag == 0) {
        iter=0;
        if ( n <= 0 || m <= 0 )
        {
            iflag= -3;
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
                double ys = w.segment(iypt + npt, n).dot(w.segment(ispt + npt, n));
                double yy = w.segment(iypt + npt, n).squaredNorm();
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

void compute_exp_Mi(int num_labels, Eigen::MatrixXd &Mi, Eigen::VectorXd &Vi) {
    // take exponential operator
    for (int m = 0; m < num_labels; m++) {
        Vi(m) = std::exp(Vi(m));
        for (int n = 0; n < num_labels; n++) {
            Mi(m, n) = std::exp(Mi(m, n));
        }
    }
}

/**
* @brief compute the log likelihood and gradient of the objective function 
*/

void compute_logli_gradient(LinCrfLBFGSTransitionState<MutableArrayHandle<double> >& state, MappedColumnVector& featureTuple){
    int feature_size = static_cast<int>(featureTuple.size());
    int seq_len = static_cast<int>(featureTuple(feature_size-1)) + 1;
    Eigen::MatrixXd betas(state.num_labels, seq_len);
    Eigen::VectorXd scale(seq_len);
    Eigen::MatrixXd Mi(state.num_labels,state.num_labels);
    Eigen::VectorXd Vi(state.num_labels);
    Eigen::VectorXd alpha(state.num_labels);
    Eigen::VectorXd next_alpha(state.num_labels);
    Eigen::VectorXd temp(state.num_labels);
    Eigen::VectorXd ExpF(state.num_features);
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
        while (index-5>=0 && featureTuple(index-1) == i) {
            int f_type =  (int)featureTuple(index-5);
            int prev_index =  (int)featureTuple(index-4);
            int curr_index =  (int)featureTuple(index-3);
            int f_index =  (int)featureTuple(index-2);
            if (f_type == 2) {// state feature
                Vi(curr_index) += state.coef(f_index);
            } else if (f_type == 1) { // edge feature
                Mi(prev_index, curr_index) += state.coef(f_index);
            }
            index-=6;
        }

        compute_exp_Mi(state.num_labels, Mi, Vi);

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
        int ori_index = index;
        while (((index+5) <= (feature_size-1)) && featureTuple(index+4) == j) {
            int f_type =  (int)featureTuple(index);
            int prev_index =  (int)featureTuple(index+1);
            int curr_index =  (int)featureTuple(index+2);
            int f_index =  (int)featureTuple(index+3);
            if (f_type == 2) {// state feature
                Vi(curr_index) += state.coef(f_index);
            } else if (f_type == 1) { //edge feature
                Mi(prev_index, curr_index) += state.coef(f_index);
            }
            index+=6;
        }

        compute_exp_Mi(state.num_labels, Mi, Vi);

        if(j>0) {
            temp = alpha;
            next_alpha=Mi*temp;
            next_alpha=next_alpha.cwiseProduct(Vi);
        } else {
            next_alpha=Vi;
        }


        index = ori_index;
        while (((index+5) <= (feature_size-1)) && featureTuple(index+4) == j) {
            int f_type =  (int)featureTuple(index);
            int prev_index =  (int)featureTuple(index+1);
            int curr_index =  (int)featureTuple(index+2);
            int f_index =  (int)featureTuple(index+3);
            int exist = (int)featureTuple(index+5);
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

}

/**
 * @brief Compute the log likelihood and gradient vector for each tuple
 */
AnyType
lincrf_lbfgs_step_transition::run(AnyType &args) {
    LinCrfLBFGSTransitionState<MutableArrayHandle<double> > state = args[0];
    MappedColumnVector featureTuple = args[1].getAs<MappedColumnVector>();
    if (state.numRows == 0) {
        state.initialize(*this, static_cast<uint32_t>(args[2].getAs<double>()), static_cast<uint32_t>(args[3].getAs<double>()));
        if (!args[4].isNull()) {
            LinCrfLBFGSTransitionState<ArrayHandle<double> > previousState = args[4];
            state = previousState;
            state.reset();
        }
    }
    state.numRows++;
    compute_logli_gradient(state, featureTuple);
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
 * @brief Perform the licrf_lbfgs final step
 */
AnyType
lincrf_lbfgs_step_final::run(AnyType &args) {
// We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    LinCrfLBFGSTransitionState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    // To avoid overfitting, penalize the likelihood with a spherical Gaussian 
    // weight prior
    double sigma_square = 100;
    state.loglikelihood -= state.coef.dot(state.coef)/ 2 * sigma_square;
    state.grad -= state.coef;

    // the lbfgs minimize function, we want to maximize loglikelihood 
    state.loglikelihood = state.loglikelihood * -1;
    state.grad = -state.grad;

    double eps = 1.0e-6; //accuracy of the solution to be found
    double xtol = 1.0e-16; //an estimate of the machine precision

    assert((state.m > 0) && (state.m <= state.num_features) && (eps >= 0.0));

    LBFGS instance(state);
    // lbfgs optimization
    instance.lbfgs(state.num_features, state.m, state.loglikelihood, state.grad, eps, xtol);
    instance.save_state(state);

    if(instance.iflag < 0)  throw std::logic_error("lbfgs failed");

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
internal_lincrf_lbfgs_converge::run(AnyType &args) {
    LinCrfLBFGSTransitionState<ArrayHandle<double> > state = args[0];
    return state.lbfgs_state(6);
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
    MutableMappedColumnVector coef(
        inAllocator.allocateArray<double>(incoef.size()));
    coef = incoef;

    // Return all coefficients, loglikelihood in a tuple
    AnyType tuple;
    tuple << coef << loglikelihood;
    return tuple;
}

} // namespace crf

} // namespace modules

} // namespace madlib
