/* ----------------------------------------------------------------------- *//** 
 * 
 * @file newton.hpp
 * 
 * Generic implementaion of Newton's method, in the fashion of
 * user-definied aggregates. They should be called by actually database
 * functions, after arguments are properly parsed.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#ifndef MADLIB_MODULES_CONVEX_ALGO_NEWTON_HPP_
#define MADLIB_MODULES_CONVEX_ALGO_NEWTON_HPP_

namespace madlib {

namespace modules {

namespace convex {

// use Eigen
using namespace madlib::dbal::eigen_integration;
    
// The reason for using ConstState instead of const State to reduce the
// template type list: flexibility to high-level for mutability control
// More: cast<ConstState>(MutableState) may not always work
template <class State, class ConstState, class Task>
class Newton {
public:
    typedef State state_type;
    typedef ConstState const_state_type;
    typedef typename Task::tuple_type tuple_type;
    typedef typename Task::model_type model_type;

    static void transition(state_type &state, const tuple_type &tuple);
    static void merge(state_type &state, const_state_type &otherState);
    static void final(state_type &state);
};

template <class State, class ConstState, class Task>
void
Newton<State, ConstState, Task>::transition(state_type &state,
        const tuple_type &tuple) {
    // accumulating the gradient & hessian
    Task::gradient(
            state.task.model,
            tuple.indVar,
            tuple.depVar,
            state.algo.gradient);
    Task::hessian(
            state.task.model,
            tuple.indVar,
            tuple.depVar,
            state.algo.hessian);
}

template <class State, class ConstState, class Task>
void
Newton<State, ConstState, Task>::merge(state_type &state,
        const_state_type &otherState) {
    // merging accumulated gradient & hessian
    state.algo.gradient += otherState.algo.gradient;
    state.algo.hessian += otherState.algo.hessian;
}

template <class State, class ConstState, class Task>
void
Newton<State, ConstState, Task>::final(state_type &state) {
    // w_{k+1} = w_k - H_k^{-1} g_k
    // instead of computing the inverse explicitly,
    // we solve H_k p_k = g_k, and w_{k+1} = w_k - 1 * p_k

    // solve in place, subject to change if gradient is somehow needed
    // ldlt decomposition is chosen for its numerical stability, see 
    // http://eigen.tuxfamily.org/dox-3.0/TutorialLinearAlgebra.html
    state.algo.gradient = state.algo.hessian.ldlt().solve(state.algo.gradient);
    state.task.model -= state.algo.gradient;
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

