
#include "dbconnector/dbconnector.hpp"
#include "state/igd.hpp"
#include "share/shared_utils.hpp"
#include <limits>

namespace madlib {
namespace modules {
namespace elastic_net {

template <class Model>
class Igd
{
  public:
    static AnyType igd_transition (AnyType& args, const Allocator& inAllocator);
    static AnyType igd_merge (AnyType& args);
    static AnyType igd_final (AnyType& args);
    static AnyType igd_state_diff (AnyType& args);
    static AnyType igd_result (AnyType& args);

  private:
    static double p_abs (CVector& v, double r);
    static void link_fn (CVector& theta, CVector& w, double p);
    static double sign (const double & x);
};

// ------------------------------------------------------------------------

/**
   @brief Perform IGD transition step

   It is called for each tuple.

   The input AnyType has 9 args: state. ind_var, dep_var,
   pre_state, lambda, alpha, dimension, stepsize, totalrows
*/
template <class Model>
AnyType Igd<Model>::igd_transition (AnyType& args, const Allocator& inAllocator)
{
    IgdState<MutableArrayHandle<double> > state = args[0];
    double lambda = args[4].getAs<double>();
    double stepsize = args[7].getAs<double>();

    // initialize the state if working on the first tuple
    if (state.numRows == 0)
    {
        if (!args[3].isNull())
        {
            IgdState<ArrayHandle<double> > pre_state = args[3];
            state.allocate(inAllocator, pre_state.dimension);
            state = pre_state;
        }
        else
        {
            double alpha = args[5].getAs<double>();
            int dimension = args[6].getAs<int>();

            int total_rows = args[8].getAs<int>();

            state.allocate(inAllocator, dimension);
            state.step_decay = args[11].getAs<double>();
            state.stepsize = stepsize * exp(state.step_decay);
            state.alpha = alpha;
            state.totalRows = total_rows;
            state.xmean = args[9].getAs<MappedColumnVector>();
            state.ymean = args[10].getAs<double>();
            // dual vector theta
            state.theta.setZero();
            state.p = 2 * log(double(state.dimension));
            state.lambda = lambda;
            state.q = state.p / (state.p - 1);
            link_fn(state.theta, state.coef, state.p);

            Model::init_intercept (state); // initialize intercept
        }

        //state.loss = 0.;

        if (state.lambda != lambda)
        {
            state.lambda = lambda;
            state.stepsize = stepsize * exp(state.step_decay);
        }

        // state.stepsize = state.stepsize / exp(state.step_decay);

        state.numRows = 0; // resetting
    }

    state.stepsize = state.stepsize / exp(state.step_decay);

    try {
        MappedColumnVector x = args[1].getAs<MappedColumnVector>();
        double y;

        Model::get_y(y, args);

        ColumnVector gradient(state.dimension); // gradient for coef only, not for intercept
        Model::compute_gradient(gradient, state, x, y);

        double a = state.stepsize / static_cast<double>(state.totalRows);
        double b = state.stepsize * state.alpha * state.lambda
            / static_cast<double>(state.totalRows);
        for (uint32_t i = 0; i < state.dimension; i++)
        {
            // step 1
            state.theta(i) -= a * gradient(i);
            double step1_sign = sign(state.theta(i));
            // step 2
            state.theta(i) -= b * sign(state.theta(i));
            // set to 0 if the value crossed zero during the two steps
            if (step1_sign != sign(state.theta(i))) state.theta(i) = 0;
        }

        link_fn(state.theta, state.coef, state.p);

        Model::update_intercept(state, x, y); // intercept is updated separately

        Model::update_loglikelihood(state, x, y);
        // state.loss += r * r / 2.;
        state.numRows ++;
    } catch(const ArrayWithNullException &e) {
      //  warning("Input array most likely contains NULL, skipping this array.");
    } catch(const std::invalid_argument &ia) {
        //warning("Input array is invalid (with NULL values), skipping this array.");
    }

    return state;
}

// ------------------------------------------------------------------------

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
template <class Model>
AnyType Igd<Model>::igd_merge (AnyType& args)
{
    IgdState<MutableArrayHandle<double> > state1 = args[0];
    IgdState<ArrayHandle<double> > state2 = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (state1.numRows == 0) { return state2; }
    else if (state2.numRows == 0) { return state1; }

    double totalNumRows = static_cast<double>(state1.numRows + state2.numRows);
    state1.coef *= static_cast<double>(state1.numRows) /
        static_cast<double>(state2.numRows);
    state1.coef += state2.coef;
    state1.coef *= static_cast<double>(state2.numRows) /
        static_cast<double>(totalNumRows);

    Model::merge_intercept(state1, state2);
    state1.loglikelihood += state2.loglikelihood;

    // The following numRows update, cannot be put above, because the coef
    // averaging depends on their original values
    state1.numRows += state2.numRows;

    if (state1.stepsize > state2.stepsize)
        state1.stepsize = state2.stepsize;

    return state1;
}

// ------------------------------------------------------------------------

/**
 * @brief Perform the final step
 */
template <class Model>
AnyType Igd<Model>::igd_final (AnyType& args)
{
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    IgdState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0) return Null();

    Model::update_intercept_final (state); // intercept is updated separately

    // generate new theta values
    link_fn(state.coef, state.theta, state.q);

    // compute the final loglikelihood value from the accumulated loss value
    double loss_value;
    loss_value = state.loglikelihood / static_cast<double>(state.numRows * 2);
    double sum_sqr_coef = 0;
    double sum_abs_coef = 0;
    for (uint32_t i = 0; i < state.dimension; i++){
        sum_sqr_coef += state.coef(i) * state.coef(i);
        sum_abs_coef += std::abs(state.coef(i));
    }
    state.loglikelihood = -(loss_value + state.lambda *
                                ((1 - state.alpha) * sum_sqr_coef / 2 +
                                 state.alpha * sum_abs_coef));
    return state;
}

// ------------------------------------------------------------------------

/**
 * @brief Return the difference in RMSE between two states
 */
template <class Model>
AnyType Igd<Model>::igd_state_diff (AnyType& args)
{
    IgdState<ArrayHandle<double> > state1 = args[0];
    IgdState<ArrayHandle<double> > state2 = args[1];
    double diff;
    diff = std::abs(std::abs(state1.loglikelihood) -
                    std::abs(state2.loglikelihood));
    // double tmp;
    // double diff_sum = 0;
    // uint32_t n = state1.coef.rows();
    // for (uint32_t i = 0; i < n; i++)
    // {
    //     diff = std::abs(state1.coef(i) - state2.coef(i));
    //     tmp = std::abs(state2.coef(i));
    //     if (tmp != 0) diff /= tmp;
    //     diff_sum += diff;
    // }

    // // deal with intercept
    // diff = std::abs(state1.intercept - state2.intercept);
    // tmp = std::abs(state2.intercept);
    // if (tmp != 0) diff /= tmp;
    // diff_sum += diff;
    // return diff_sum / (n + 1);
    return diff;
}

// ------------------------------------------------------------------------

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
template <class Model>
AnyType Igd<Model>::igd_result (AnyType& args)
{
    IgdState<MutableArrayHandle<double> > state = args[0];
    MappedColumnVector x2 = args[1].getAs<MappedColumnVector>();
    double threshold = args[2].getAs<double>();
    double tolerance = args[3].getAs<double>();

    ColumnVector norm_coef(state.dimension);
    double avg = 0;
    for (uint32_t i = 0; i < state.dimension; i++)
    {
        norm_coef(i) = state.coef(i) * sqrt(x2(i) - state.xmean(i) * state.xmean(i));
        avg += fabs(norm_coef(i));
    }
    avg /= state.dimension;

    for (uint32_t i = 0; i < state.dimension; i++)
        if (fabs(norm_coef(i)/avg) < threshold || fabs(norm_coef(i)) < tolerance)
            state.coef(i) = 0;

    AnyType tuple;
    tuple << static_cast<double>(state.intercept)
          << state.coef
          << static_cast<double>(state.lambda);

    return tuple;
}

// ------------------------------------------------------------------------
template <class Model>
inline double Igd<Model>::p_abs (CVector& v, double r)
{
    double sum = 0;
    for (int i = 0; i < v.size(); i++)
        if (v(i) != 0)
            sum += pow(fabs(v(i)), r);
    return pow(sum, 1./r);
}

// ------------------------------------------------------------------------

// p-form link function, q = p/(p-1)
// For inverse function, jut replace w with theta and q with p
template <class Model>
inline void Igd<Model>::link_fn (CVector& theta, CVector& w, double p)
{
    double abs_theta = p_abs(theta, p);
    if (fabs(abs_theta) <= std::numeric_limits<double>::denorm_min())
    {
        for (int i = 0; i < theta.size(); i++) w(i) = 0;
        return;
    }

    double denominator = pow(abs_theta, p - 2);

    for (int i = 0; i < theta.size(); i++)
        if (fabs(theta(i)) <= std::numeric_limits<double>::denorm_min())
            w(i) = 0;
        else
            w(i) = sign(theta(i)) * pow(fabs(theta(i)), p - 1)
                / denominator;
}

// ------------------------------------------------------------------------

// sign of a number
template <class Model>
inline double Igd<Model>::sign (const double & x)
{
    if (x > 0)
        return 1;
    else if (x < 0)
        return -1;
    else
        return 0;
}

} // namespace elastic_net
} // namespace modules
} // namespace madlib
