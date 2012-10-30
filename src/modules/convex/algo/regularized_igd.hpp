/* ----------------------------------------------------------------------- *//** 
 *
 * @file regularized_igd.hpp
 *
 * Generic implementaion of IGD with regularization, in the fashion of
 * user-definied aggregates. They should be called by actually database
 * functions, after arguments are properly parsed.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#ifndef MADLIB_MODULES_CONVEX_ALGO_REGULARIZED_IGD_HPP_
#define MADLIB_MODULES_CONVEX_ALGO_REGULARIZED_IGD_HPP_

namespace madlib {

namespace modules {

namespace convex {

// use Eigen
using namespace madlib::dbal::eigen_integration;
    
template <class State, class Task, class Regularizer>
class RegularizedIGD {
public:
    typedef State state_type;
    typedef typename Task::tuple_type tuple_type;
    typedef typename Task::model_type model_type;

    static void transition(state_type &state, const tuple_type &tuple);
};

template <class State, class Task, class Regularizer>
void
RegularizedIGD<State, Task, Regularizer>::transition(state_type &state,
        const tuple_type &tuple) {
    // local copy
    // FIXME putting ColumnVector here is a wrong level of abstraction
    // perhaps template param is better (Task::model_local_type)
    // gradient may not be of type ColumnVector, e.g., LMF
    // but definitely not model_type, which is reference type (not local copy)
    state.algo.gradient.setZero();

    Task::gradient(
            state.algo.incrModel,
            tuple.indVar,
            tuple.depVar,
            state.algo.gradient);
    Regularizer::gradient(
            state.algo.incrModel,
            state.task.lambda / static_cast<double>(state.task.totalRows), // amortizing lambda
            state.algo.gradient);
    
    // apply to the model directly
    state.algo.incrModel -= state.task.stepsize * state.algo.gradient;
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

