/******************************************************************************
** DARWIN: A FRAMEWORK FOR MACHINE LEARNING RESEARCH AND DEVELOPMENT
** Copyright (c) 2007-2012, Stephen Gould
** All rights reserved.
**
******************************************************************************
** FILENAME:    drwnOptimizer.h
** AUTHOR(S):   Stephen Gould <stephen.gould@anu.edu.au>
**
*****************************************************************************/

#pragma once

#include "/usr/include/eigen3/Eigen/Dense"
#include "/usr/include/eigen3/Eigen/Core"
/*!
** \brief Interface for solving large-scale unconstrained optimization problems
** using L-BFGS.
**
** Implements optimization using L-BFGS code is based on an implementation
** by Jorge Nocedal:
**
**  \li J. Nocedal. Updating  Quasi-Newton  Matrices  with  Limited  Storage
**      (1980), Mathematics of Computation 35, pp. 773-782.
**  \li D.C. Liu and J. Nocedal. On the  Limited  Memory  Method  for  Large
**      Scale  Optimization  (1989),  Mathematical  Programming  B,  45,  3,
**      pp. 503-528.
**
** The derived class must override functions \ref objective and \ref gradient.
** It should also implement the \ref objectiveAndGradient function for
** efficiency, and may override the \ref monitor function. The \ref monitor
** function can assume that \ref _x contains the current estimate for the
** solution. Other functions should use the input argument.
**
** This class tries to \b minimize the objective function define by \ref
** objective.
**
** The following simple example optimizes the one-dimensional function
** \f$f(x) = (x - 2)(x - 4)\f$.
** \code
**   class MyObjective : public drwnOptimizer
**   {
**     MyObjective() : drwnOptimizer(1) { }
**     ~MyObjective() { }
**
**     double objective(const double *x) const {
**       return (x[0] - 2.0) * (x[0] - 4.0);
**     }
**
**     void gradient(const double *x, double *df) const {
**       df[0] = 2.0 * (x[0] - 3.0);
**     }
**   };
**
**   int main()
**   {
**     MyObjective objFunction;
**
**     double f_star = objFunction.solve(1000);
**     double x_star = objFunction[0];
**
**     cout << "f(" << x_star << ") = " << f_star << endl;
**     return 0;
**   }
** \endcode
*/
class drwnOptimizer {
 public:
    unsigned _n;  //!< dimension of optimization problem (i.e., \f$\mathbb{R}^n\f$)
    double *_x;   //!< current feasible solution in \f$\mathbb{R}^n\f$
    double *_df;  //!< gradient at _x  in \f$\mathbb{R}^n\f$

 public:
    //! default constructor
    drwnOptimizer();
    //! construct a problem with dimension \b n
    drwnOptimizer(unsigned n);
    //! copy constructor
    drwnOptimizer(const drwnOptimizer& o);
    virtual ~drwnOptimizer();

    //! initialize an optimization problem of size \b n possibly with feasible
    //! starting point \b x \f$\mathbb{R}^n\f$ (or zero)
    void initialize(unsigned n, const double *x = NULL);
    //! initialize an optimization problem at feasible starting point \b x in
    //! \f$\mathbb{R}^n\f$ (or zero)
    void initialize(const double *x = NULL);

    //! solve the optimization problem for up to \b maxiter iterations to precision \b tol
    //! calls \ref monitor function after each iteration if \b bMonitor is \a true
    double solve(unsigned maxiter, double tol = 1.0e-3, bool bMonitor = false);

    //! returns value of objective function at point \b x
    virtual double objective(const double *x) const = 0;
    //! populates gradient of objective function at point \b x
    virtual void gradient(const double *x, double *df) const = 0;
    //! returns value of objective function and populates gradient \b df at point \b x
    virtual double objectiveAndGradient(const double *x, double *df) const {
	gradient(x, df); return objective(x);
    }

    //! dimension of optimization problem
    inline unsigned size() const { return _n; }
    //! returns the \a i-th component of the current solution
    inline double operator[](unsigned i) const { return _x[i]; }
    //! returns a reference to the \a i-th component of the current solution
    inline double& operator[](unsigned i) { return _x[i]; }

    //! callback for each iteration during optimization (if \b bMonitor is \a true)
    virtual void monitor(unsigned iter, double objValue);

 protected:
    ///@cond
    enum  drwnLBFGSResult {
        DRWN_LBFGS_ERROR, DRWN_LBFGS_MAX_ITERS, DRWN_LBFGS_CONVERGED_F,
        DRWN_LBFGS_CONVERGED_G, DRWN_LBFGS_CONVERGED_X
    };

    drwnLBFGSResult lbfgsMinimize(int m, unsigned maxiter,
        double epsg, double epsf, double epsx, bool bMonitor);
    bool lbfgsSearch(double &f, const Eigen::VectorXd &s,
        double& stp, Eigen::VectorXd& diag);
    bool lbfgsStep(double& stx, double& fx, double& dx,
        double& sty, double& fy, double& dy,
        double& stp, const double& fp, const double& dp,
        bool& brackt, const double& stmin, const double& stmax);
    ///@endcond
};


