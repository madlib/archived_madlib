
#include "dbconnector/dbconnector.hpp"
#include "elastic_net_binomial_igd.hpp"
#include "state/igd.hpp"
#include "elastic_net_optimizer_igd.hpp"
#include "share/shared_utils.hpp"
#include <limits>

namespace madlib {
namespace modules {
namespace elastic_net {

/*
  This class contains specific methods needed by Binomial model using IGD
 */
class BinomialIgd
{
  public:
    static void init_intercept (IgdState<MutableArrayHandle<double> >& state);
    static void get_y (double& y, AnyType& args);
    static void compute_gradient (ColumnVector& gradient,
                                  IgdState<MutableArrayHandle<double> >& state,
                                  MappedColumnVector& x, double y);
    static void update_intercept (IgdState<MutableArrayHandle<double> >& state,
                                  MappedColumnVector& x, double y);
    static void merge_intercept (IgdState<MutableArrayHandle<double> >& state1,
                                 IgdState<ArrayHandle<double> >& state2);
    static void update_loglikelihood(IgdState<MutableArrayHandle<double> >& state,
                                     MappedColumnVector& x, double y);
    static void update_intercept_final (IgdState<MutableArrayHandle<double> >& state);

};

// ------------------------------------------------------------------------

inline void BinomialIgd::init_intercept (IgdState<MutableArrayHandle<double> >& state)
{
    state.intercept = 0;
}

// ------------------------------------------------------------------------
// extract dependent variable from args
inline void BinomialIgd::get_y (double& y, AnyType& args)
{
    y = args[2].getAs<bool>() ? 1. : -1.;
}

// ------------------------------------------------------------------------

inline void BinomialIgd::compute_gradient (ColumnVector& gradient,
                                           IgdState<MutableArrayHandle<double> >& state,
                                           MappedColumnVector& x, double y)
{
    double r = state.intercept + sparse_dot(state.coef, x);
    double u;

    if (y > 0)
        u = - 1. / (1. + std::exp(r));
    else
        u = 1. / (1. + std::exp(-r));

    for (uint32_t i = 0; i < state.dimension; i++)
        gradient(i) = x(i) * u;
}

// ------------------------------------------------------------------------

inline void BinomialIgd::update_intercept (IgdState<MutableArrayHandle<double> >& state,
                                           MappedColumnVector& x, double y)
{
    double r = state.intercept + sparse_dot(state.coef, x);
    double u;

    if (y > 0)
        u = - 1. / (1. + std::exp(r));
    else
        u = 1. / (1. + std::exp(-r));

    state.intercept -= state.stepsize * u;
}

// ------------------------------------------------------------------------

// do nothing
inline void BinomialIgd::merge_intercept  (IgdState<MutableArrayHandle<double> >& state1,
                                           IgdState<ArrayHandle<double> >& state2)
{
    double totalNumRows = static_cast<double>(state1.numRows + state2.numRows);
    state1.intercept = state1.intercept * static_cast<double>(state1.numRows) /
        static_cast<double>(state2.numRows);
    state1.intercept += state2.intercept;
    state1.intercept = state1.intercept * static_cast<double>(state2.numRows) /
        static_cast<double>(totalNumRows);
}

// ------------------------------------------------------------------------

/**
   @brief Compute log-likelihood for one data point in binomial models
*/
inline void BinomialIgd::update_loglikelihood(
        IgdState<MutableArrayHandle<double> >& state,
        MappedColumnVector& x, double y) {

    double r = state.intercept + sparse_dot(state.coef, x);
    if (y > 0)
        state.loglikelihood += std::log(1 + std::exp(-r));
    else
        state.loglikelihood +=  std::log(1 + std::exp(r));
}
// ------------------------------------------------------------------------

inline void BinomialIgd::update_intercept_final (IgdState<MutableArrayHandle<double> >& state)
{
    // absolutely do nothing
    // because everything is already done in merge and transition
    (void)state;
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
binomial_igd_transition::run (AnyType& args)
{
    return Igd<BinomialIgd>::igd_transition(args, *this);
}

// ------------------------------------------------------------------------

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
binomial_igd_merge::run (AnyType& args)
{
    return Igd<BinomialIgd>::igd_merge(args);
}

// ------------------------------------------------------------------------

/**
 * @brief Perform the final step
 */
AnyType
binomial_igd_final::run (AnyType& args)
{
    return Igd<BinomialIgd>::igd_final(args);
}

// ------------------------------------------------------------------------

/**
 * @brief Return the difference in RMSE between two states
 */
AnyType
__binomial_igd_state_diff::run (AnyType& args)
{
    return Igd<BinomialIgd>::igd_state_diff(args);
}

// ------------------------------------------------------------------------

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
__binomial_igd_result::run (AnyType& args)
{
    return Igd<BinomialIgd>::igd_result(args);
}

} // namespace elastic_net
} // namespace modules
} // namespace madlib
