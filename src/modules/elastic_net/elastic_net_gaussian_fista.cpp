
#include "dbconnector/dbconnector.hpp"
#include "elastic_net_gaussian_fista.hpp"
#include "state/fista.hpp"
#include "elastic_net_optimizer_fista.hpp"
#include "share/shared_utils.hpp"
#include <math.h>       /* pow */

namespace madlib {
namespace modules {
namespace elastic_net {

/*
  This class contains specific methods needed by Gaussian model using FISTA
 */
class GaussianFista
{
  public:
    static void initialize(FistaState<MutableArrayHandle<double> >& state);
    static void get_y(double& y, AnyType& args);
    static void normal_transition(FistaState<MutableArrayHandle<double> >& state,
                                   MappedColumnVector& x, double y);
    static void active_transition(FistaState<MutableArrayHandle<double> >& state,
                                   MappedColumnVector& x, double y);

    // update the backtracking coef
    static void update_b_intercept(FistaState<MutableArrayHandle<double> >& state);

    // update the loglikelihood
    static void update_loglikelihood(
        FistaState<MutableArrayHandle<double> >& state,
        MappedColumnVector& x, double y);

    // update the proxy coef
    static void update_y_intercept(FistaState<MutableArrayHandle<double> >& state,
                                    double old_tk);

    static void update_y_intercept_final(FistaState<MutableArrayHandle<double> >& state);

    static void merge_intercept(FistaState<MutableArrayHandle<double> >& state1,
                                 FistaState<ArrayHandle<double> >& state2);

  private:
    static void backtracking_transition(FistaState<MutableArrayHandle<double> >& state,
                                         MappedColumnVector& x, double y);
};

// ------------------------------------------------------------------------

inline void GaussianFista::update_y_intercept_final (
    FistaState<MutableArrayHandle<double> >& state)
{
    state.gradient_intercept = state.gradient_intercept / static_cast<double>(state.totalRows);
}
// -----------------------------------------------------------------------------

/**
   @brief Compute log-likelihood for one data point in gaussian models
*/
inline void GaussianFista::update_loglikelihood (
        FistaState<MutableArrayHandle<double> >& state,
        MappedColumnVector& x, double y) {
    state.loglikelihood += pow(y - state.intercept - sparse_dot(state.coef, x), 2);
}

// ------------------------------------------------------------------------

inline void GaussianFista::merge_intercept(
        FistaState<MutableArrayHandle<double> >& state1,
        FistaState<ArrayHandle<double> >& state2) {

    state1.gradient_intercept += state2.gradient_intercept;
}

// ------------------------------------------------------------------------
// extract dependent variable from args
inline void GaussianFista::get_y (double& y, AnyType& args)
{
    y = args[2].getAs<double>();
}

// ------------------------------------------------------------------------

inline void GaussianFista::update_b_intercept (FistaState<MutableArrayHandle<double> >& state)
{
    state.b_intercept = state.intercept_y - state.stepsize * state.gradient_intercept;
}

// ------------------------------------------------------------------------

inline void GaussianFista::update_y_intercept (FistaState<MutableArrayHandle<double> >& state,
                                               double old_tk)
{
    state.intercept_y = state.b_intercept + (old_tk - 1) * (state.b_intercept - state.intercept)
        / state.tk;
}

// ------------------------------------------------------------------------
// initialize state values for the first iteration only
inline void GaussianFista::initialize (FistaState<MutableArrayHandle<double> >& state)
{
    state.coef.setZero();
    state.coef_y.setZero();
    state.intercept = 0;
    state.intercept_y = 0;
    state.loglikelihood = 0;
}

// ------------------------------------------------------------------------
// just compute fn and Qfn
inline void GaussianFista::backtracking_transition (FistaState<MutableArrayHandle<double> >& state,
                                                    MappedColumnVector& x, double y)
{
    // during backtracking, always use b_coef and b_intercept
    double r = y - state.b_intercept - sparse_dot(state.b_coef, x);
    state.fn += r * r * 0.5;

    // Qfn only need to be calculated once in each backtracking
    if (state.backtracking == 1)
    {
        r = y - state.intercept_y - sparse_dot(state.coef_y, x);
        state.Qfn += r * r * 0.5;
    }
}

// ------------------------------------------------------------------------
/*
  Transition part when no active set is used
 */
inline void GaussianFista::normal_transition (FistaState<MutableArrayHandle<double> >& state,
                                              MappedColumnVector& x, double y)
{
    if (state.backtracking == 0)
    {
        double r = y - state.intercept_y - sparse_dot(state.coef_y, x);
        for (uint32_t i = 0; i < state.dimension; i++)
            state.gradient(i) += - x(i) * r;

        // update gradient
        state.gradient_intercept += - r;
    }
    else
        backtracking_transition(state, x, y);
}

// ------------------------------------------------------------------------

/*
  Transition part when active set is used
 */
inline void GaussianFista::active_transition (FistaState<MutableArrayHandle<double> >& state,
                                              MappedColumnVector& x, double y)
{
    if (state.backtracking == 0) {
        double r = y - state.intercept_y - sparse_dot(state.coef_y, x);
        for (uint32_t i = 0; i < state.dimension; i++)
            if (state.coef_y(i) != 0)
                state.gradient(i) += - x(i) * r;

        state.gradient_intercept += - r;
    } else
        backtracking_transition(state, x, y);
}

// ------------------------------------------------------------------------
// ------------------------------------------------------------------------
// ------------------------------------------------------------------------

/*
  The following are the functions that are actually called by SQL
*/

/**
   @brief Perform FISTA transition step

   It is called for each tuple of (x, y)
*/
AnyType gaussian_fista_transition::run (AnyType& args)
{
    return Fista<GaussianFista>::fista_transition(args, *this);
}

// ------------------------------------------------------------------------
/**
   @brief Perform Merge transition steps
*/
AnyType gaussian_fista_merge::run (AnyType& args)
{
    return Fista<GaussianFista>::fista_merge(args);
}

// ------------------------------------------------------------------------
/**
   @brief Perform the final computation
*/
AnyType gaussian_fista_final::run (AnyType& args)
{
    return Fista<GaussianFista>::fista_final(args);
}

// ------------------------------------------------------------------------

/**
 * @brief Return the difference in RMSE between two states
 */
AnyType __gaussian_fista_state_diff::run (AnyType& args)
{
    return Fista<GaussianFista>::fista_state_diff(args);
}

// ------------------------------------------------------------------------

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType __gaussian_fista_result::run (AnyType& args)
{
    return Fista<GaussianFista>::fista_result(args);
}

}
}
}
