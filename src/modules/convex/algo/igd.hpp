/* ----------------------------------------------------------------------- *//** 
 *
 * @file igd.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_CONVEX_ALGO_IGD_HPP_
#define MADLIB_CONVEX_ALGO_IGD_HPP_

namespace madlib {

namespace modules {

namespace convex {

template <class State, class ConstState, class Task>
class IGD {
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
IGD<State, ConstState, Task>::transition(state_type &state, const tuple_type &tuple) {
    // moved to upper-level
    // s.algo.numRows ++;

    // apply to the model directly
    Task::gradientInPlace(
            state.algo.incrModel,
            tuple.indVar,
            tuple.depVar,
            state.task.stepsize);
}

template <class State, class ConstState, class Task>
void
IGD<State, ConstState, Task>::merge(state_type &state, const_state_type &otherState) {
    // model averaging, weighted by rows seen
    double totalNumRows = state.algo.numRows + otherState.algo.numRows;
    state.algo.incrModel *= (1. * state.algo.numRows) / otherState.algo.numRows;
    state.algo.incrModel += otherState.algo.incrModel;
    state.algo.incrModel *= (1. * otherState.algo.numRows) / totalNumRows;
}

template <class State, class ConstState, class Task>
void
IGD<State, ConstState, Task>::final(state_type &state) {
    state.task.model = state.algo.incrModel;
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

