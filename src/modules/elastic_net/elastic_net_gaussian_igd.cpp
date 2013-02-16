
#include "dbconnector/dbconnector.hpp"
#include "elastic_net_gaussian_igd.hpp"
#include "state/igd.hpp"
#include <limits>

namespace madlib {
namespace modules {
namespace elastic_net {

typedef HandleTraits<MutableArrayHandle<double> >::ColumnVectorTransparentHandleMap CVector;

/*
  dot product of sparese vector
 */
static double sparse_dot (CVector& coef, MappedColumnVector& x)
{
    double sum = 0;
    for (int i = 0; i < x.size(); i++)
        if (coef(i) != 0) sum += coef(i) * x(i);
    return sum;
}

static double sparse_dot (CVector& coef, CVector& x)
{
    double sum = 0;
    for (int i = 0; i < x.size(); i++)
        if (coef(i) != 0) sum += coef(i) * x(i);
    return sum;
}

// sign of a number
static double sign(const double & x) {
    if (x > 0)
        return 1;
    else if (x < 0)
        return -1;
    else
        return 0;
}

// ------------------------------------------------------------------------
// Need divided-by-zero type check

static double p_abs (CVector& v, double r)
{
    double sum = 0;
    for (int i = 0; i < v.size(); i++)
        if (v(i) != 0)
            sum += pow(fabs(v(i)), r);
    return pow(sum, 1./r);
}

// p-form link function, q = p/(p-1)
// For inverse function, jut replace w with theta and q with p 
static void link_fn (CVector& theta, CVector& w, double p)
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

/**
   @brief Perform IGD transition step

   It is called for each tuple.

   The input AnyType has 9 args: state. ind_var, dep_var,
   pre_state, lambda, alpha, dimension, stepsize, totalrows
*/
AnyType
gaussian_igd_transition::run (AnyType& args)
{
    IgdState<MutableArrayHandle<double> > state = args[0];
    double lambda = args[4].getAs<double>();

    // initialize the state if working on the first tuple
    if (state.numRows == 0)
    {
        if (!args[3].isNull())
        {
            IgdState<ArrayHandle<double> > pre_state = args[3];
            state.allocate(*this, pre_state.dimension);
            state = pre_state;
        }
        else
        {
            double alpha = args[5].getAs<double>();
            int dimension = args[6].getAs<int>();
            double stepsize = args[7].getAs<double>();
            int total_rows = args[8].getAs<int>();

            state.allocate(*this, dimension);
            state.stepsize = stepsize;
            state.alpha = alpha;
            state.totalRows = total_rows;
            state.xmean = args[9].getAs<MappedColumnVector>();
            state.ymean = args[10].getAs<double>();
            // dual vector theta
            state.theta.setZero();
            state.p = 2 * log(state.dimension);
            state.q = state.p / (state.p - 1);
            link_fn(state.theta, state.coef, state.p);
            state.intercept = state.ymean - dot(state.coef, state.xmean);
        }
         
        //state.loss = 0.;
        state.lambda = lambda;
        state.numRows = 0; // resetting
    }
    
    MappedColumnVector x = args[1].getAs<MappedColumnVector>();
    double y = args[2].getAs<double>();

    // Now do the transition step
    double wx = sparse_dot(state.incrCoef, x) + state.incrIntercept;
    double r = wx - y;

    ColumnVector gradient(state.dimension);
    gradient = r * (x - state.xmean) + (1 - state.alpha) * state.lambda
        * state.incrCoef;

    double a = state.stepsize / state.totalRows;
    double b = state.stepsize * state.alpha * state.lambda
        / state.totalRows;
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

    link_fn(state.theta, state.incrCoef, state.p);

    state.incrIntercept = state.ymean - sparse_dot(state.incrCoef, state.xmean);

    // state.loss += r * r / 2.;
    state.numRows ++;

    return state;
}

// ------------------------------------------------------------------------

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
gaussian_igd_merge::run (AnyType& args)
{
    IgdState<MutableArrayHandle<double> > state1 = args[0];
    IgdState<ArrayHandle<double> > state2 = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (state1.numRows == 0) { return state2; }
    else if (state2.numRows == 0) { return state1; }

    double totalNumRows = static_cast<double>(state1.numRows + state2.numRows);
    state1.incrCoef *= static_cast<double>(state1.numRows) /
        static_cast<double>(state2.numRows);
    state1.incrCoef += state2.incrCoef;
    state1.incrCoef *= static_cast<double>(state2.numRows) /
        static_cast<double>(totalNumRows);
    
    //state1.loss += state2.loss;
    
    // The following numRows update, cannot be put above, because the coef
    // averaging depends on their original values
    state1.numRows += state2.numRows;

    return state1;
}

// ------------------------------------------------------------------------

/**
 * @brief Perform the final step
 */
AnyType
gaussian_igd_final::run (AnyType& args)
{
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    IgdState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0) return Null(); 

    // finalizing
    //state.coef = link_fn(state.theta, state.p);
    state.coef = state.incrCoef;
    state.intercept = state.ymean - sparse_dot(state.coef, state.xmean);

    link_fn(state.coef, state.theta, state.q);
  
    return state;
}

// ------------------------------------------------------------------------

/**
 * @brief Return the difference in RMSE between two states
 */
AnyType
__gaussian_igd_state_diff::run (AnyType& args)
{
    IgdState<ArrayHandle<double> > state1 = args[0];
    IgdState<ArrayHandle<double> > state2 = args[1];

    double diff_sum = 0;
    uint32_t n = state1.coef.rows();
    for (uint32_t i = 0; i < n; i++)
    {
        double diff = std::abs(state1.coef(i) - state2.coef(i));
        double tmp = std::abs(state1.coef(i));
        if (tmp > 1) diff /= tmp;
        diff_sum += diff;
    }

    return diff_sum / n;
}

// ------------------------------------------------------------------------

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
__gaussian_igd_result::run (AnyType& args)
{
    IgdState<MutableArrayHandle<double> > state = args[0];
    MappedColumnVector x2 = args[1].getAs<MappedColumnVector>();
    double threshold = args[2].getAs<double>();

    ColumnVector norm_coef(state.dimension);
    double avg = 0;
    for (uint32_t i = 0; i < state.dimension; i++)
    {
        norm_coef(i) = state.coef(i) * sqrt(x2(i) - state.xmean(i) * state.xmean(i));
        avg += fabs(norm_coef(i));
    }
    avg /= state.dimension;

    for (uint32_t i = 0; i < state.dimension; i++)
        if (fabs(state.coef(i)/avg) < threshold)
            state.coef(i) = 0;
        
    AnyType tuple;
    tuple << static_cast<double>(state.intercept)
          << state.coef
          << static_cast<double>(state.lambda);

    return tuple;
}
 
} // namespace elastic_net 
} // namespace modules
} // namespace madlib
