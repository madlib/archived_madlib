
#include "dbconnector/dbconnector.hpp"
#include "elastic_net_gaussian_fista.hpp"
#include "state/fista.hpp"

namespace madlib {
namespace modules {
namespace elastic_net {

typedef HandleTraits<MutableArrayHandle<double> >::ColumnVectorTransparentHandleMap CVector;

// ------------------------------------------------------------------------

/*
  The proxy function, in this case it is just the soft thresholding
 */
static void proxy (CVector& y, CVector& gradient_y, CVector& x,
                   double stepsize, double lambda)
{
    ColumnVector u = y - stepsize * gradient_y;
    for (int i = 0; i < y.size(); i++)
    {
        if (u(i) > lambda)
            x(i) = u(i) - lambda;
        else if (u(i) < - lambda)
            x(i) = u(i) + lambda;
        else
            x(i) = 0;
    }
}

// ------------------------------------------------------------------------

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

static double sparse_dot (ColumnVector& coef, CVector& x)
{
    double sum = 0;
    for (int i = 0; i < x.size(); i++)
        if (coef(i) != 0) sum += coef(i) * x(i);
    return sum;
}

static double sparse_dot (ColumnVector& coef, ColumnVector& x)
{
    double sum = 0;
    for (int i = 0; i < x.size(); i++)
        if (coef(i) != 0) sum += coef(i) * x(i);
    return sum;
}

// ------------------------------------------------------------------------

static void backtracking_transition (FistaState<MutableArrayHandle<double> >& state,
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
static void normal_transition (FistaState<MutableArrayHandle<double> >& state,
                               MappedColumnVector& x, double y)
{
    if (state.backtracking == 0)
    {
        state.gradient += - (x - state.xmean) * (y - state.intercept_y);
        for (uint32_t j = 0; j < state.dimension; j++)
            state.gradient(j) += (x(j) - state.xmean(j)) *
                sparse_dot(state.coef_y, x);
    }
    else 
        backtracking_transition(state, x, y);
}

// ------------------------------------------------------------------------

/*
  Transition part when active set is used
 */
static void active_transition (FistaState<MutableArrayHandle<double> >& state,
                               MappedColumnVector& x, double y)
{
    if (state.backtracking == 0) {
        for (uint32_t i = 0; i < state.dimension; i++)
            if (state.coef_y(i) != 0)
                state.gradient(i) += - (x(i) - state.xmean(i)) *
                    (y - state.intercept_y - sparse_dot(state.coef_y, x));
    } else 
        backtracking_transition(state, x, y);
}

// ------------------------------------------------------------------------

/**
   @brief Perform FISTA transition step

   It is called for each tuple of (x, y)
*/
AnyType gaussian_fista_transition::run (AnyType& args)
{
    FistaState<MutableArrayHandle<double> > state = args[0];
    double lambda = args[4].getAs<double>();

    // initialize the state if working on the first tuple
    if (state.numRows == 0)
    {
        if (!args[3].isNull())
        {
            FistaState<ArrayHandle<double> > pre_state = args[3];
            state.allocate(*this, pre_state.dimension);
            state = pre_state;
        }
        else
        {
            double alpha = args[5].getAs<double>();
            uint32_t dimension = args[6].getAs<uint32_t>();
            MappedColumnVector xmean = args[7].getAs<MappedColumnVector>();
            double ymean = args[8].getAs<double>();
            double tk = args[9].getAs<double>();
            int totalRows = args[10].getAs<int>();
            
            state.allocate(*this, dimension);
            state.alpha = alpha;
            state.totalRows = totalRows;
            state.ymean = ymean;
            state.tk = tk;
            state.backtracking = 0; // the first iteration is always non-backtracking
            state.max_stepsize = args[11].getAs<double>();
            state.eta = args[12].getAs<double>();

            // whether to use active-set method
            // 1 is yes, 0 is no
            state.use_active_set = args[13].getAs<int>();

            for (uint32_t i = 0; i < dimension; i++)
            {
                // initial values
                state.coef(i) = 0;
                state.coef_y(i) = 0;
                state.xmean(i) = xmean(i);
            }
            
            state.intercept = ymean;
            state.intercept_y = ymean;
            state.stepsize = state.max_stepsize;
        }

        if (state.backtracking == 0)
            state.gradient.setZero();
        else
        {
            state.fn = 0;
            if (state.backtracking == 1) state.Qfn = 0;
        }

        // lambda is changing if warm-up is used
        // so needs to update it everytime
        state.lambda = lambda;

        state.numRows = 0; // resetting

        state.is_active = args[14].getAs<int>();
    }

    MappedColumnVector x = args[1].getAs<MappedColumnVector>();
    double y = args[2].getAs<double>();

    if (state.use_active_set == 1 && state.is_active == 1)
        active_transition(state, x, y);
    else
        normal_transition(state, x, y);

    state.numRows++;

    return state;
}

// ------------------------------------------------------------------------

/**
   @brief Perform Merge transition steps
*/
AnyType gaussian_fista_merge::run (AnyType& args)
{
    FistaState<MutableArrayHandle<double> > state1 = args[0];
    FistaState<MutableArrayHandle<double> > state2 = args[1];

    if (state1.numRows == 0)
        return state2;
    else if (state2.numRows == 0)
        return state1;

    if (state1.backtracking == 0) {
        if (state1.use_active_set == 1 && state1.is_active == 1)
        {
            for (uint32_t i = 0; i < state1.dimension; i++)
                if (state1.coef_y(i) != 0)
                    state1.gradient(i) += state2.gradient(i);
        }
        else
            state1.gradient += state2.gradient;
    }
    else
    {
        state1.fn += state2.fn;

        // Qfn only need to be calculated once in each backtracking
        if (state1.backtracking == 1)
            state1.Qfn += state2.Qfn;
    }
    
    state1.numRows += state2.numRows;

    return state1;
}

// ------------------------------------------------------------------------

/**
   @brief Perform the final computation
*/
AnyType gaussian_fista_final::run (AnyType& args)
{
    FistaState<MutableArrayHandle<double> > state = args[0];
   
    // Aggregates that haven't seen any data just return Null
    if (state.numRows == 0) return Null();

    if (state.backtracking == 0) 
    {
        state.gradient = state.gradient / state.totalRows;
        double la = state.lambda * (1 - state.alpha);
        for (uint32_t i = 0; i < state.dimension; i++)
            if (state.coef_y(i) != 0)
                state.gradient(i) += la * state.coef_y(i);

        // compute the first set of coef for backtracking
        //state.stepsize = state.max_stepsize;
        state.stepsize = state.stepsize * state.eta;
        double effective_lambda = state.lambda * state.alpha * state.stepsize;
        proxy(state.coef_y, state.gradient, state.b_coef, 
              state.stepsize, effective_lambda);
        state.b_intercept = state.ymean - sparse_dot(state.b_coef, state.xmean);

        state.backtracking = 1; // will do backtracking
    }
    else
    {
        // finish computing fn and Qfn if needed
        state.fn = state.fn / state.totalRows + 0.5 * state.lambda * (1 - state.alpha)
            * sparse_dot(state.b_coef, state.b_coef);
        
        if (state.backtracking == 1)
            state.Qfn = state.Qfn / state.totalRows + 0.5 * state.lambda * (1 - state.alpha)
                * sparse_dot(state.coef_y, state.coef_y);

        ColumnVector r = state.b_coef - state.coef_y;
        double extra_Q = sparse_dot(r, state.gradient) + 0.5 * sparse_dot(r, r) / state.stepsize;
        
        if (state.fn <= state.Qfn + extra_Q) { // use last backtracking coef
            // update coef and intercept
            ColumnVector old_coef = state.coef;
            state.coef = state.b_coef;
            state.intercept = state.b_intercept;

            // update tk
            double old_tk = state.tk;
            state.tk = 0.5 * (1 + sqrt(1 + 4 * old_tk * old_tk));

            // update coef_y and intercept_y
            state.coef_y = state.coef + (old_tk - 1) * (state.coef - old_coef)
                / state.tk;
            state.intercept_y = state.ymean - sparse_dot(state.coef_y, state.xmean);
            
            state.backtracking = 0; // stop backtracking
        }
        else
        {
            state.stepsize = state.stepsize / state.eta;
            double effective_lambda = state.lambda * state.alpha * state.stepsize;
            proxy(state.coef_y, state.gradient, state.b_coef, 
                  state.stepsize, effective_lambda);
            state.b_intercept = state.ymean - sparse_dot(state.b_coef, state.xmean);

            state.backtracking++;
        }
    }
    
    return state;
}

// ------------------------------------------------------------------------

/**
 * @brief Return the difference in RMSE between two states
 */
AnyType __gaussian_fista_state_diff::run (AnyType& args)
{
    FistaState<ArrayHandle<double> > state1 = args[0];
    FistaState<ArrayHandle<double> > state2 = args[1];

    // during backtracking, do not comprae the coefficients
    // of two consecutive states
    if (state2.backtracking > 0) return 1e12;
    
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
AnyType __gaussian_fista_result::run (AnyType& args)
{
    FistaState<ArrayHandle<double> > state = args[0];
    AnyType tuple;
    
    tuple << static_cast<double>(state.intercept)
          << state.coef
          << static_cast<double>(state.lambda);

    return tuple;
}

}
}
}
