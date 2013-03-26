
/*
  Common functions that are used by both Gaussian and Binomial models
 */

#include "dbconnector/dbconnector.hpp"
#include "state/fista.hpp"
#include "share/shared_utils.hpp"

#include <cstdlib>
#include <ctime>
#include <fstream>

namespace madlib {
namespace modules {
namespace elastic_net {

template <class Model>
class Fista
{
  public:
    static AnyType fista_transition (AnyType& args, const Allocator& inAllocator);
    static AnyType fista_merge (AnyType& args);
    static AnyType fista_final (AnyType& args);
    static AnyType fista_state_diff (AnyType& args);
    static AnyType fista_result (AnyType& args);

  private:
    static void proxy (CVector& y, CVector& gradient_y, CVector& x,
                       double stepsize, double lambda);
};

// ------------------------------------------------------------------------

/*
  The proxy function, in this case it is just the soft thresholding
 */
template <class Model>
inline void Fista<Model>::proxy (CVector& y, CVector& gradient_y, CVector& x,
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

/**
   @brief Perform FISTA transition step

   It is called for each tuple of (x, y)
*/
template <class Model>
AnyType Fista<Model>::fista_transition (AnyType& args, const Allocator& inAllocator)
{
    FistaState<MutableArrayHandle<double> > state = args[0];
    double lambda = args[4].getAs<double>();

    // initialize the state if working on the first tuple
    if (state.numRows == 0)
    {
        if (!args[3].isNull())
        {
            FistaState<ArrayHandle<double> > pre_state = args[3];
            state.allocate(inAllocator, pre_state.dimension);
            state = pre_state;
        }
        else
        {
            double alpha = args[5].getAs<double>();
            uint32_t dimension = args[6].getAs<uint32_t>();
            int totalRows = args[7].getAs<int>();
            
            state.allocate(inAllocator, dimension);
            state.alpha = alpha;
            state.totalRows = totalRows;
            state.tk = 1;
            state.backtracking = 0; // the first iteration is always non-backtracking
            state.max_stepsize = args[8].getAs<double>();
            state.eta = args[9].getAs<double>();
            state.lambda = lambda;

            //dev/ --------------------------------------------------------
            // how to adaptively update stepsize
            srand48 (time(NULL));
            state.stepsize_sum = 0;
            state.iter = 0;
            //dev/ --------------------------------------------------------

            // whether to use active-set method
            // 1 is yes, 0 is no
            state.use_active_set = args[10].getAs<int>();

            Model::initialize(state, args);
            
            state.stepsize = state.max_stepsize;

            state.random_stepsize = args[12].getAs<int>();
        }

        if (state.backtracking == 0)
        {
            state.gradient.setZero();
            state.gradient_intercept = 0;
        }
        else
        {
            state.fn = 0;
            if (state.backtracking == 1) state.Qfn = 0;
        }

        // lambda is changing if warm-up is used
        // so needs to update it everytime
        if (state.lambda != lambda)
        {
            // restore initial conditions
            state.lambda = lambda;
            state.tk = 1;
            state.stepsize = state.max_stepsize;
            state.stepsize_sum = 0;
            state.iter = 0;
            state.backtracking = 0;
            state.coef_y = state.coef;
            state.intercept_y = state.intercept;
        }

        state.numRows = 0; // resetting

        state.is_active = args[11].getAs<int>();
    }

    MappedColumnVector x = args[1].getAs<MappedColumnVector>();
    double y;

    Model::get_y(y, args);

    if (state.use_active_set == 1 && state.is_active == 1)
        Model::active_transition(state, x, y);
    else
        Model::normal_transition(state, x, y);

    state.numRows++;

    return state;
}

// ------------------------------------------------------------------------

/**
   @brief Perform Merge transition steps
*/
template <class Model>
AnyType Fista<Model>::fista_merge (AnyType& args)
{
    FistaState<MutableArrayHandle<double> > state1 = args[0];
    FistaState<ArrayHandle<double> > state2 = args[1];

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

        Model::merge_intercept(state1, state2);
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
template <class Model>
AnyType Fista<Model>::fista_final (AnyType& args)
{
    FistaState<MutableArrayHandle<double> > state = args[0];
   
    // Aggregates that haven't seen any data just return Null
    if (state.numRows == 0) return Null();

    // std::ofstream of;
    // of.open("/Users/qianh1/workspace/tests/feature_ElasticNet/stepsize.txt", std::ios::app);
    // of << "stepsize = " << state.stepsize
    //    << ", backtracking = " << state.backtracking
    //    << ", is_active = " << state.is_active
    //    << std::endl;
    // of.close();
    
    if (state.backtracking == 0) 
    {
        state.gradient = state.gradient / state.totalRows;
        double la = state.lambda * (1 - state.alpha);
        for (uint32_t i = 0; i < state.dimension; i++)
            if (state.coef_y(i) != 0)
                state.gradient(i) += la * state.coef_y(i);

        Model::update_y_intercept_final(state);

        //dev/ --------------------------------------------------------
        // How to adaptively update stepsize
        // set the initial value for backtracking stepsize
        if (state.random_stepsize == 1)
        {
            double stepsize_avg;
            if (state.iter == 0) stepsize_avg = 0;
            else stepsize_avg = state.stepsize_sum / state.iter;

            double p = 1. / (1 + exp(0.5 * (log(state.stepsize/state.max_stepsize) - stepsize_avg) / log(state.eta)));
            double r = drand48();

            // there is a non-zero probability to increase stepsize
            if (r < p)
                state.stepsize = state.stepsize * state.eta;

            //if (state.is_active == 0) state.stepsize = state.stepsize * state.eta;
        }
        
        //dev/ -------------------------------------------------------- 
        
        double effective_lambda = state.lambda * state.alpha * state.stepsize;
        //if (state.is_active == 1) effective_lambda *= 1.1;
        proxy(state.coef_y, state.gradient, state.b_coef, 
              state.stepsize, effective_lambda);
        Model::update_b_intercept(state);

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
        if (state.gradient_intercept != 0)
            extra_Q += - 0.5 * state.gradient_intercept * state.gradient_intercept * state.stepsize;
     
        if (state.fn <= state.Qfn + extra_Q) { // use last backtracking coef
            // update tk
            double old_tk = state.tk;
            state.tk = 0.5 * (1 + sqrt(1 + 4 * old_tk * old_tk));

            // update coef_y and intercept_y
            state.coef_y = state.b_coef + (old_tk - 1) * (state.b_coef - state.coef)
                / state.tk;
            Model::update_y_intercept(state, old_tk);

            // this must behind Model::update_y_intercept,
            // because in binomial case, state.intercept is
            // the old value
            state.coef = state.b_coef;
            state.intercept = state.b_intercept;
            
            state.backtracking = 0; // stop backtracking

            //dev/ --------------------------------------------------------
            // how to adaptively update stepsize
            if (state.random_stepsize == 1)
            {
                state.stepsize_sum += log(state.stepsize) - log(state.max_stepsize);
                state.iter++;
            }
            // std::ofstream of;
            // of.open("/Users/qianh1/workspace/tests/feature_ElasticNet/stepsize.txt", std::ios::app);
            // of << "Proceed, stepsize = " << state.stepsize << std::endl;
            // of.close();
            //dev/ --------------------------------------------------------
        }
        else
        {
            state.stepsize = state.stepsize / state.eta;
            double effective_lambda = state.lambda * state.alpha * state.stepsize;
            //if (state.is_active == 1) effective_lambda *= 1.1;
            proxy(state.coef_y, state.gradient, state.b_coef, 
                  state.stepsize, effective_lambda);
            Model::update_b_intercept(state);
            
            state.backtracking++;
        }
    }
    
    return state;
}

// ------------------------------------------------------------------------

/**
 * @brief Return the difference in RMSE between two states
 */
template <class Model>
AnyType Fista<Model>::fista_state_diff (AnyType& args)
{
    FistaState<ArrayHandle<double> > state1 = args[0];
    FistaState<ArrayHandle<double> > state2 = args[1];

    // during backtracking, do not comprae the coefficients
    // of two consecutive states
    if (state2.backtracking > 0) return 1e12;
    
    double diff_sum = 0;
    double diff, tmp;
    uint32_t n = state1.coef.rows();
    for (uint32_t i = 0; i < n; i++)
    {
        diff = std::abs(state1.coef(i) - state2.coef(i));
        tmp = std::abs(state2.coef(i));
        if (tmp != 0) diff /= tmp;
        diff_sum += diff;
    }

    // deal with intercept
    diff = std::abs(state1.intercept - state2.intercept);
    tmp = std::abs(state2.intercept);
    if (tmp != 0) diff /= tmp;
    diff_sum += diff;
    
    return diff_sum / (n + 1);
}

// ------------------------------------------------------------------------

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
template <class Model>
AnyType Fista<Model>::fista_result (AnyType& args)
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
