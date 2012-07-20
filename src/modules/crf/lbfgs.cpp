/*****************************************************************************
** DARWIN: A FRAMEWORK FOR MACHINE LEARNING RESEARCH AND DEVELOPMENT
** Copyright (c) 2007-2012, Stephen Gould
** All rights reserved.
**
******************************************************************************
** FILENAME:    drwnOptimizer.cpp
** AUTHOR(S):   Stephen Gould <stephen.gould@anu.edu.au>
**
*****************************************************************************/

// C++ standard library
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <cmath>
#include <string.h>
#include <assert.h>
#include <limits>
#include <iomanip>
//#include <dbconnector/dbconnector.hpp>
//#include <modules/shared/HandleTraits.hpp>

//#include "/usr/include/eigen3/Eigen/Dense"
//#include "/usr/include/eigen3/Eigen/Core"

using namespace std;
using namespace Eigen;
//using namespace dbal::eigen_integration;


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
    static const double XTOL = 100.0 * numeric_limits<double>::min();
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
                   double epsg, double epsf, double epsx, unsigned  _n, VectorXd x, double f,
                   VectorXd g, VectorXd diag, VectorXd w)
{
    assert((m > 0) && (m <= (int)_n));
    assert((epsg >= 0.0) && (epsf >= 0.0) && (epsx >= 0.0));

    VectorXd xold(_n);

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


