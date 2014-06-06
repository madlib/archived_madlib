
#include "dbconnector/dbconnector.hpp"
#include "elastic_net_gaussian_igd.hpp"
#include "state/igd.hpp"
#include "elastic_net_optimizer_igd.hpp"
#include "share/shared_utils.hpp"
#include <limits>

namespace madlib {
namespace modules {
namespace elastic_net {

/*
  This class contains specific methods needed by Gaussian model using IGD
 */
class GaussianIgd
{
  public:
    static void init_intercept (IgdState<MutableArrayHandle<double> >& state);
    static void get_y (double& y, AnyType& args);
    static void compute_gradient (ColumnVector& gradient,
                                  IgdState<MutableArrayHandle<double> >& state,
                                  MappedColumnVector& x, double y);
    static void update_intercept (IgdState<MutableArrayHandle<double> >& state,
                                  MappedColumnVector& x, double y);
    // update the loglikelihood
    static void update_loglikelihood (
        IgdState<MutableArrayHandle<double> >& state,
        MappedColumnVector& x, double y);
    static void merge_intercept (IgdState<MutableArrayHandle<double> >& state1,
                                 IgdState<ArrayHandle<double> >& state2);
    static void update_intercept_final (IgdState<MutableArrayHandle<double> >& state);

};

// ------------------------------------------------------------------------
// extract dependent variable from args
inline void GaussianIgd::get_y (double& y, AnyType& args)
{
    y = args[2].getAs<double>();
}

// ------------------------------------------------------------------------

inline void GaussianIgd::init_intercept (IgdState<MutableArrayHandle<double> >& state)
{
    state.intercept = state.ymean - dot(state.coef, state.xmean);
}

// ------------------------------------------------------------------------

inline void GaussianIgd::compute_gradient (
    ColumnVector& gradient,
    IgdState<MutableArrayHandle<double> >& state,
    MappedColumnVector& x, double y)
{
    double wx = sparse_dot(state.coef, x) + state.intercept;
    double r = wx - y;

    gradient = r * (x - state.xmean) + (1 - state.alpha) * state.lambda
        * state.coef;
}

// ------------------------------------------------------------------------

inline void GaussianIgd::update_intercept (
    IgdState<MutableArrayHandle<double> >& state,
    MappedColumnVector& x, double y)
{
    // avoid unused parameter warning,
    // actually does absolutely nothing
    (void)x;
    (void)y;

    state.intercept = state.ymean - sparse_dot(state.coef, state.xmean);
}
// ------------------------------------------------------------------------

/**
   @brief Compute log-likelihood for one data point in gaussian models
*/
inline void GaussianIgd::update_loglikelihood (
        IgdState<MutableArrayHandle<double> >& state,
        MappedColumnVector& x, double y) {
    state.loglikelihood += pow(y - state.intercept - sparse_dot(state.coef, x), 2);
}

// ------------------------------------------------------------------------


// do nothing
inline void GaussianIgd::merge_intercept  (
    IgdState<MutableArrayHandle<double> >& state1,
    IgdState<ArrayHandle<double> >& state2)
{
    // Avoid unused parameter warning, the function does nothing
    // This function is included here since the igd optimizer uses this function
    // for the binomial case.
    (void)state1;
    (void)state2;
}

// ------------------------------------------------------------------------

inline void GaussianIgd::update_intercept_final (
    IgdState<MutableArrayHandle<double> >& state)
{
    state.intercept = state.ymean - sparse_dot(state.coef, state.xmean);
}

// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------

/*
  The following are the functions that are actually called by SQL
*/

/**
   @brief Perform IGD transition step

   It is called for each tuple.

   The input AnyType has 9 args: state. ind_var, dep_var,
   pre_state, lambda, alpha, dimension, stepsize, totalrows
*/
AnyType
gaussian_igd_transition::run (AnyType& args)
{
    return Igd<GaussianIgd>::igd_transition(args, *this);
}

// ------------------------------------------------------------------------

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
gaussian_igd_merge::run (AnyType& args)
{
    return Igd<GaussianIgd>::igd_merge(args);
}

// ------------------------------------------------------------------------

/**
 * @brief Perform the final step
 */
AnyType
gaussian_igd_final::run (AnyType& args)
{
    return Igd<GaussianIgd>::igd_final(args);
}

// ------------------------------------------------------------------------

/**
 * @brief Return the difference in RMSE between two states
 */
AnyType
__gaussian_igd_state_diff::run (AnyType& args)
{
    return Igd<GaussianIgd>::igd_state_diff(args);
}

// ------------------------------------------------------------------------

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
__gaussian_igd_result::run (AnyType& args)
{
    return Igd<GaussianIgd>::igd_result(args);
}

} // namespace elastic_net
} // namespace modules
} // namespace madlib
