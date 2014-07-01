
#include "dbconnector/dbconnector.hpp"
#include "elastic_net_binomial_fista.hpp"
#include "state/fista.hpp"
#include "elastic_net_optimizer_fista.hpp"
#include "share/shared_utils.hpp"

namespace madlib {
namespace modules {
namespace elastic_net {

/*
  This class contains specific methods needed by Gaussian model using FISTA
 */
class BinomialFista
{
  public:
    static void initialize(FistaState<MutableArrayHandle<double> >& state);
    static void get_y(double& y, AnyType& args);
    static void normal_transition(FistaState<MutableArrayHandle<double> >& state,
                                  MappedColumnVector& x, double y);
    static void active_transition(FistaState<MutableArrayHandle<double> >& state,
                                  MappedColumnVector& x, double y);

    static void update_b_intercept(FistaState<MutableArrayHandle<double> >& state);
    static void update_loglikelihood(FistaState<MutableArrayHandle<double> >& state,
                                     MappedColumnVector& x, double y);

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

inline void BinomialFista::update_y_intercept_final(FistaState<MutableArrayHandle<double> >& state)
{
    state.gradient_intercept = state.gradient_intercept / static_cast<double>(state.totalRows);
}
// -----------------------------------------------------------------------------

/**
   @brief Compute log-likelihood for one data point in binomial models
*/
inline void BinomialFista::update_loglikelihood(
        FistaState<MutableArrayHandle<double> >& state,
        MappedColumnVector& x, double y) {

    double r = state.intercept + sparse_dot(state.coef, x);
    if (y > 0)
        state.loglikelihood += std::log(1 + std::exp(-r));
    else
        state.loglikelihood +=  std::log(1 + std::exp(r));
}

// ------------------------------------------------------------------------

inline void BinomialFista::merge_intercept(
        FistaState<MutableArrayHandle<double> >& state1,
        FistaState<ArrayHandle<double> >& state2) {
    state1.gradient_intercept += state2.gradient_intercept;
}

// ------------------------------------------------------------------------

inline void BinomialFista::initialize(FistaState<MutableArrayHandle<double> >& state)
{
    state.coef.setZero();
    state.coef_y.setZero();
    state.intercept = 0;
    state.intercept_y = 0;
}

// ------------------------------------------------------------------------
// extract dependent variable from args
inline void BinomialFista::get_y(double& y, AnyType& args)
{
    y = args[2].getAs<bool>() ? 1. : -1.;
}

// ------------------------------------------------------------------------

inline void BinomialFista::normal_transition(FistaState<MutableArrayHandle<double> >& state,
                                              MappedColumnVector& x, double y)
{
    if (state.backtracking == 0)
    {
        double r = state.intercept_y + sparse_dot(state.coef_y, x);
        double u;

        if (y > 0)
            u = - 1. / (1. + std::exp(r));
        else
            u = 1. / (1. + std::exp(-r));

        for (uint32_t i = 0; i < state.dimension; i++)
            state.gradient(i) += x(i) * u;

        // update gradient
        state.gradient_intercept += u;
    }
    else
        backtracking_transition(state, x, y);
}

// ------------------------------------------------------------------------

inline void BinomialFista::active_transition(FistaState<MutableArrayHandle<double> >& state,
                                              MappedColumnVector& x, double y)
{
    if (state.backtracking == 0) // Compute gradient for active set
    {
        double r = state.intercept_y + sparse_dot(state.coef_y, x);
        double u;

        if (y > 0)
            u = - 1. / (1. + std::exp(r));
        else
            u = 1. / (1. + std::exp(-r));

        for (uint32_t i = 0; i < state.dimension; i++)
            if (state.coef_y(i) != 0)
                state.gradient(i) += x(i) * u;

        // always update intercept
        state.gradient_intercept += u;
    }
    else
        backtracking_transition(state, x, y);
}

// ------------------------------------------------------------------------

inline void BinomialFista::backtracking_transition(FistaState<MutableArrayHandle<double> >& state,
                                                    MappedColumnVector& x, double y)
{
    // during backtracking, always use b_coef and b_intercept
    double r = state.b_intercept + sparse_dot(state.b_coef, x);

    if (y > 0)
        state.fn += std::log(1 + std::exp(-r));
    else
        state.fn += std::log(1 + std::exp(r));

    // Qfn only need to be calculated once in each backtracking
    if (state.backtracking == 1)
    {
        r = state.intercept_y + sparse_dot(state.coef_y, x);
        if (y > 0)
            state.Qfn += std::log(1 + std::exp(-r));
        else
            state.Qfn += std::log(1 + std::exp(r));
    }
}

// ------------------------------------------------------------------------

inline void BinomialFista::update_b_intercept (FistaState<MutableArrayHandle<double> >& state)
{
    state.b_intercept = state.intercept_y - state.stepsize * state.gradient_intercept;
}

// ------------------------------------------------------------------------

inline void BinomialFista::update_y_intercept (FistaState<MutableArrayHandle<double> >& state,
                                               double old_tk)
{
    state.intercept_y = state.b_intercept + (old_tk - 1) * (state.b_intercept - state.intercept)
        / state.tk;
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
AnyType binomial_fista_transition::run (AnyType& args)
{
    return Fista<BinomialFista>::fista_transition(args, *this);
}

// ------------------------------------------------------------------------
/**
   @brief Perform Merge transition steps
*/
AnyType binomial_fista_merge::run (AnyType& args)
{
    return Fista<BinomialFista>::fista_merge(args);
}

// ------------------------------------------------------------------------
/**
   @brief Perform the final computation
*/
AnyType binomial_fista_final::run (AnyType& args)
{
    return Fista<BinomialFista>::fista_final(args);
}

// ------------------------------------------------------------------------

/**
 * @brief Return the difference in RMSE between two states
 */
AnyType __binomial_fista_state_diff::run (AnyType& args)
{
    return Fista<BinomialFista>::fista_state_diff(args);
}

// ------------------------------------------------------------------------

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType __binomial_fista_result::run (AnyType& args)
{
    return Fista<BinomialFista>::fista_result(args);
}

}
}
}
