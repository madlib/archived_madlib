/* ----------------------------------------------------------------------- *//**
 *
 * @file ridge_newton.cpp
 *
 * @brief Logistic Regression functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#include "ridge_newton.hpp"

#include "task/ols.hpp"
#include "task/l2.hpp"
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
        OLS<GLMModel, GLMTuple, GLMHessian > > OLSNewtonAlgorithm;

typedef L2<GLMModel, GLMHessian > Ridge;

/**
 * @brief Perform the ridge regression transition step
 *
 * Called for each tuple.
 */
AnyType
ridge_newton_transition::run(AnyType &args) {
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
        // a heck...
        double lambda = args[5].getAs<double>();
        state.algo.loss = lambda;
    }

    // tuple
    using madlib::dbal::eigen_integration::MappedColumnVector;
    GLMTuple tuple;
    tuple.indVar.rebind(args[1].getAs<MappedColumnVector>().memoryHandle());
    tuple.depVar = args[2].getAs<double>();

    // Now do the transition step
    OLSNewtonAlgorithm::transition(state, tuple);
    state.algo.numRows ++;

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
ridge_newton_merge::run(AnyType &args) {
    GLMNewtonState<MutableArrayHandle<double> > stateLeft = args[0];
    GLMNewtonState<ArrayHandle<double> > stateRight = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.algo.numRows == 0) { return stateRight; }
    else if (stateRight.algo.numRows == 0) { return stateLeft; }

    // Merge states together
    OLSNewtonAlgorithm::merge(stateLeft, stateRight);
    stateLeft.algo.numRows += stateRight.algo.numRows;

    return stateLeft;
}

/**
 * @brief Perform the ridge regression final step
 */
AnyType
ridge_newton_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    GLMNewtonState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.algo.numRows == 0) { return Null(); }

    // finalizing
    double lambda = state.algo.loss;
    Ridge::gradient(state.task.model, lambda, state.algo.gradient);
    Ridge::hessian(state.task.model, lambda, state.algo.hessian);
    OLSNewtonAlgorithm::final(state);
    
    return state;
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_ridge_newton_result::run(AnyType &args) {
    GLMNewtonState<ArrayHandle<double> > state = args[0];

    return state.task.model;
}

/**
 * @brief Return the prediction reselt
 */
AnyType
ridge_newton_predict::run(AnyType &args) {
    using madlib::dbal::eigen_integration::MappedColumnVector;
    MappedColumnVector model = args[0].getAs<MappedColumnVector>();
    MappedColumnVector indVar = args[1].getAs<MappedColumnVector>();

    return OLS<MappedColumnVector, GLMTuple>::predict(model, indVar);
}

} // namespace convex

} // namespace modules

} // namespace madlib

