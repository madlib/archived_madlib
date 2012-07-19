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
        double epsg, double epsf, double epsx, bool bMonitor);
    static bool lbfgsSearch(double &f, const Eigen::VectorXd &s,
        double& stp, Eigen::VectorXd& diag);
    static bool lbfgsStep(double& stx, double& fx, double& dx,
        double& sty, double& fy, double& dy,
        double& stp, const double& fp, const double& dp,
        bool& brackt, const double& stmin, const double& stmax);
    ///@endcond
};
