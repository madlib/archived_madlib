/* ----------------------------------------------------------------------- *//** 
 *
 * @file conjugate_gradient.hpp
 *
 * Generic implementaion of conjugate gradient, in the fashion of
 * user-definied aggregates. They should be called by actually database
 * functions, after arguments are properly parsed.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_ALGO_CONJUGATE_GRADIENT_HPP_
#define MADLIB_MODULES_CONVEX_ALGO_CONJUGATE_GRADIENT_HPP_

#include <dbconnector/dbconnector.hpp>

namespace madlib {

namespace modules {

namespace convex {

// use Eigen
using namespace madlib::dbal::eigen_integration;
    
// The reason for using ConstState instead of const State to reduce the
// template type list: flexibility to high-level for mutability control
// More: cast<ConstState>(MutableState) may not always work
template <class State, class ConstState, class Task>
class ConjugateGradient {
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
ConjugateGradient<State, ConstState, Task>::transition(state_type &state,
        const tuple_type &tuple) {
    // accumulating the gradient
    Task::gradient(
            state.task.model,
            tuple.indVar,
            tuple.depVar,
            state.algo.incrGradient);
}

template <class State, class ConstState, class Task>
void
ConjugateGradient<State, ConstState, Task>::merge(state_type &state,
        const_state_type &otherState) {
    // merging accumulated gradient
    state.algo.incrGradient += otherState.algo.incrGradient;
}

template <class State, class ConstState, class Task>
void
ConjugateGradient<State, ConstState, Task>::final(state_type &state) {
    // mapping for design document
    // p_{k-1}: task.direction
    // g_{k-1}: task.gradient
    // w_k:     task.model
    // g_k:     algo.incrGradient
    //
    // This function is to apply conjugate gradient algorithm to properly
    // update p, g, and w

    // updating direction
    if (state.task.iteration == 0) {
        // conjugate gradient starts from steepest direction
        state.task.direction = -state.algo.incrGradient;
    } else {
        // Alternativelly, we can use Polak-Ribière
        // double beta = dot(state.algo.incrGradient, 
        //         state.algo.incrGradient - state.task.gradient) /
        //     dot(state.task.gradient, state.task.gradient);

        // or Hestenes-Stiefel
        // ColumnVector gradDifference = state.algo.incrGradient -
        //     state.task.gradient;
        // double beta = dot(state.algo.incrGradient, gradDifference) /
        //    dot(gradDifference, state.task.direction);

        // or Fletcher–Reeves method
        // double beta = dot(state.algo.incrGradient, state.algo.incrGradient) /
        //     dot(state.task.gradient, state.task.gradient);

        // Dai–Yuan method
        // This method is chosen due to lack of mechanism for stepsize line
        // search, which is required for other alternatives to guarantee
        // descent progress. See Theorem 4.1 in Hager and Zhang, "A Survey
        // of Nonlinear Conjugate Gradient Methods"
        double denumerator = dot(state.algo.incrGradient - state.task.gradient,                    
                state.task.direction);
        if (denumerator != 0.0) {
            double beta = dot(state.algo.incrGradient, state.algo.incrGradient) / 
                denumerator;

            // updating direction (p_k = -g_k + \beta * p_{k-1}
            state.task.direction *= beta;
            state.task.direction -= state.algo.incrGradient;

            // restart if p_k is not a descent direction (p_k^T g_k < 0)
            if (dot(state.task.direction, state.algo.incrGradient) >= 0.) {
                state.task.direction = -state.algo.incrGradient;
            }
        } else {
            state.task.direction = -state.algo.incrGradient;
        }
    }

    // updating gradient
    state.task.gradient = state.algo.incrGradient;

    // updating model
    state.task.model += state.task.stepsize * state.task.direction;
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

