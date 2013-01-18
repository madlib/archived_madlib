/* ----------------------------------------------------------------------- *//**
 *
 * @file logit_newton.cpp
 *
 * @brief Logistic Regression functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#include "logit_newton.hpp"

#include "task/logit.hpp"
#include "algo/newton.hpp"
#include "algo/loss.hpp"

#include "type/tuple.hpp"
#include "type/model.hpp"
#include "type/hessian.hpp"
#include "type/state.hpp"

namespace madlib {

namespace modules {

namespace convex {

// This 2 classes contain public static methods that can be called
typedef Newton<GLMNewtonState<MutableArrayHandle<double> >, GLMNewtonState<ArrayHandle<double> >,
        Logit<GLMModel, GLMTuple, GLMHessian > > LogitNewtonAlgorithm;

typedef Loss<GLMNewtonState<MutableArrayHandle<double> >, GLMNewtonState<ArrayHandle<double> >,
        Logit<GLMModel, GLMTuple, GLMHessian > > LogitLossAlgorithm;

/**
 * @brief Perform the logistic regression transition step
 *
 * Called for each tuple.
 */
AnyType
logit_newton_transition::run(AnyType &args) {
    // The real state. 
    // For the first tuple: args[0] is nothing more than a marker that 
    // indicates that we should do some initial operations.
    // For other tuples: args[0] holds the computation state until last tuple
    GLMNewtonState<MutableArrayHandle<double> > state = args[0];

    // initilize the state if first tuple
    if (state.algo.numRows == 0) {
        if (!args[3].isNull()) {
            GLMNewtonState<ArrayHandle<double> > previousState = args[3];
            state.allocate(*this, previousState.task.dimension);
            state = previousState;
        } else {
            // configuration parameters
            uint16_t dimension = args[4].getAs<uint16_t>();

            state.allocate(*this, dimension); // with zeros
        }
        // resetting in either case
        state.reset();
    }

    // tuple
    using madlib::dbal::eigen_integration::MappedColumnVector;
    GLMTuple tuple;
    tuple.indVar.rebind(args[1].getAs<MappedColumnVector>().memoryHandle());
    tuple.depVar = args[2].getAs<bool>() ? 1. : -1.;

    // Now do the transition step
    LogitNewtonAlgorithm::transition(state, tuple);
    LogitLossAlgorithm::transition(state, tuple);
    state.algo.numRows ++;

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
logit_newton_merge::run(AnyType &args) {
    GLMNewtonState<MutableArrayHandle<double> > stateLeft = args[0];
    GLMNewtonState<ArrayHandle<double> > stateRight = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.algo.numRows == 0) { return stateRight; }
    else if (stateRight.algo.numRows == 0) { return stateLeft; }

    // Merge states together
    LogitNewtonAlgorithm::merge(stateLeft, stateRight);
    LogitLossAlgorithm::merge(stateLeft, stateRight);
    // The following numRows update, cannot be put above, because the model
    // averaging depends on their original values
    stateLeft.algo.numRows += stateRight.algo.numRows;

    return stateLeft;
}

/**
 * @brief Perform the logistic regression final step
 */
AnyType
logit_newton_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    GLMNewtonState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.algo.numRows == 0) { return Null(); }

    // finalizing
    LogitNewtonAlgorithm::final(state);
    // LogitLossAlgorithm::final(state); // empty function call causes a warning
    
    // debug code
    dberr << "loss: " << state.algo.loss << std::endl;
 
    return state;
}

/**
 * @brief Return the difference in RMSE between two states
 */
AnyType
internal_logit_newton_distance::run(AnyType &args) {
    GLMNewtonState<ArrayHandle<double> > stateLeft = args[0];
    GLMNewtonState<ArrayHandle<double> > stateRight = args[1];

    return std::abs((stateLeft.algo.loss - stateRight.algo.loss)
            / stateRight.algo.loss);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_logit_newton_result::run(AnyType &args) {
    GLMNewtonState<ArrayHandle<double> > state = args[0];

    AnyType tuple;
    tuple << state.task.model
        << static_cast<double>(state.algo.loss);

    return tuple;
}

/**
 * @brief Return the prediction reselt
 */
AnyType
logit_newton_predict::run(AnyType &args) {
    using madlib::dbal::eigen_integration::MappedColumnVector;
    MappedColumnVector model = args[0].getAs<MappedColumnVector>();
    MappedColumnVector indVar = args[1].getAs<MappedColumnVector>();

    double p = Logit<MappedColumnVector, GLMTuple>::predict(model, indVar);

    return (p > 0.5);
}

} // namespace convex

} // namespace modules

} // namespace madlib

