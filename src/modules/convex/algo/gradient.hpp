/* ----------------------------------------------------------------------- *//** 
 *
 * @file gradient.hpp
 *
 * Generic implementaion of computing the norm of the gradient in the fashion 
 * of user-definied aggregates. They should be called by actually database 
 * functions, after arguments are properly parsed.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#ifndef MADLIB_MODULES_CONVEX_ALGO_GRADIENT_HPP_
#define MADLIB_MODULES_CONVEX_ALGO_GRADIENT_HPP_

namespace madlib {

namespace modules {

namespace convex {

// use Eigen
using namespace madlib::dbal::eigen_integration;
    
// The reason for using ConstState instead of const State to reduce the
// template type list: flexibility to high-level for mutability control
// More: cast<ConstState>(MutableState) may not always work
template <class State, class ConstState, class Task>
class Gradient {
public:
    typedef State state_type;
    typedef ConstState const_state_type;
    typedef typename Task::tuple_type tuple_type;
    typedef typename Task::model_type model_type;

    static void transition(state_type &state, const tuple_type &tuple);
    static void merge(state_type &state, const_state_type &otherState);
};

template <class State, class ConstState, class Task>
void
Gradient<State, ConstState, Task>::transition(state_type &state,
        const tuple_type &tuple) {
    Task::gradient(
            state.task.model, 
            tuple.indVar,
            tuple.depVar,
            state.algo.gradient);
}

template <class State, class ConstState, class Task>
void
Gradient<State, ConstState, Task>::merge(state_type &state,
        const_state_type &otherState) {
    state.algo.gradient += otherState.algo.gradient;
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif
