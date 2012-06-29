/* ----------------------------------------------------------------------- *//** 
 *
 * @file loss.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_CONVEX_ALGO_LOSS_HPP_
#define MADLIB_CONVEX_ALGO_LOSS_HPP_

namespace madlib {

namespace modules {

namespace convex {

template <class State, class ConstState, class Task>
class Loss {
public:
    typedef State state_type;
    typedef ConstState const_state_type;
    typedef typename Task::tuple_type tuple_type;
    typedef typename Task::model_type model_type;

    static void transition(state_type &state, const tuple_type &tuple);
    static void merge(state_type &state, const_state_type &otherState);
    static void final(state_type &state);
};

/* side-effect the state */
template <class State, class ConstState, class Task>
void
Loss<State, ConstState, Task>::transition(state_type &state, const tuple_type &tuple) {
    // apply to the model directly
    state.algo.loss += Task::loss(
            state.task.model, // HAYING: this is the POINT!!! "task." NOT "algo."
            tuple.indVar,
            tuple.depVar);
}

template <class State, class ConstState, class Task>
void
Loss<State, ConstState, Task>::merge(state_type &state, const_state_type &otherState) {
    state.algo.loss += otherState.algo.loss;
}

template <class State, class ConstState, class Task>
void
Loss<State, ConstState, Task>::final(state_type &state) {
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

