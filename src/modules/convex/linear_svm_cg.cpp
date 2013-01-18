/* ----------------------------------------------------------------------- *//**
 *
 * @file linear_svm_cg.cpp
 *
 * @brief Linear Support Vector Machine functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#include "linear_svm_cg.hpp"

#include "task/linear_svm.hpp"
#include "algo/conjugate_gradient.hpp"
#include "algo/loss.hpp"

#include "type/tuple.hpp"
#include "type/model.hpp"
#include "type/state.hpp"

namespace madlib {

namespace modules {

namespace convex {

// This 2 classes contain public static methods that can be called
typedef ConjugateGradient<GLMCGState<MutableArrayHandle<double> >,
        GLMCGState<ArrayHandle<double> >, LinearSVM<GLMModel, GLMTuple > >
            LinearSVMCGAlgorithm;

typedef Loss<GLMCGState<MutableArrayHandle<double> >,
        GLMCGState<ArrayHandle<double> >, LinearSVM<GLMModel, GLMTuple > >
            LinearSVMLossAlgorithm;

/**
 * @brief Perform the linear support vector machine transition step
 *
 * Called for each tuple.
 */
AnyType
linear_svm_cg_transition::run(AnyType &args) {
    // The real state. 
    // For the first tuple: args[0] is nothing more than a marker that 
    // indicates that we should do some initial operations.
    // For other tuples: args[0] holds the computation state until last tuple
    GLMCGState<MutableArrayHandle<double> > state = args[0];

    // initilize the state if first tuple
    if (state.algo.numRows == 0) {
        if (!args[3].isNull()) {
            GLMCGState<ArrayHandle<double> > previousState = args[3];
            state.allocate(*this, previousState.task.dimension);
            state = previousState;
        } else {
            // configuration parameters
            uint32_t dimension = args[4].getAs<uint32_t>();
            double stepsize = args[5].getAs<double>();

            state.allocate(*this, dimension); // with zeros
            state.task.stepsize = stepsize;
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
    LinearSVMCGAlgorithm::transition(state, tuple);
    LinearSVMLossAlgorithm::transition(state, tuple);
    state.algo.numRows ++;

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
linear_svm_cg_merge::run(AnyType &args) {
    GLMCGState<MutableArrayHandle<double> > stateLeft = args[0];
    GLMCGState<ArrayHandle<double> > stateRight = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.algo.numRows == 0) { return stateRight; }
    else if (stateRight.algo.numRows == 0) { return stateLeft; }

    // Merge states together
    LinearSVMCGAlgorithm::merge(stateLeft, stateRight);
    LinearSVMLossAlgorithm::merge(stateLeft, stateRight);
    // The following numRows update, cannot be put above, because the model
    // averaging depends on their original values
    stateLeft.algo.numRows += stateRight.algo.numRows;

    return stateLeft;
}

/**
 * @brief Perform the linear support vector machine final step
 */
AnyType
linear_svm_cg_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    GLMCGState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.algo.numRows == 0) { return Null(); }

    // finalizing
    LinearSVMCGAlgorithm::final(state);
    // LinearSVMLossAlgorithm::final(state); // empty function call causes a warning
    state.task.iteration ++;
    
    // debug code
    dberr << "loss: " << state.algo.loss << std::endl;
 
    return state;
}

/**
 * @brief Return the difference in RMSE between two states
 */
AnyType
internal_linear_svm_cg_distance::run(AnyType &args) {
    GLMCGState<ArrayHandle<double> > stateLeft = args[0];
    GLMCGState<ArrayHandle<double> > stateRight = args[1];

    return std::abs((stateLeft.algo.loss - stateRight.algo.loss)
            / stateRight.algo.loss);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_linear_svm_cg_result::run(AnyType &args) {
    GLMCGState<ArrayHandle<double> > state = args[0];

    AnyType tuple;
    tuple << state.task.model
        << static_cast<double>(state.algo.loss);

    return tuple;
}

/**
 * @brief Return the prediction reselt
 */
AnyType
linear_svm_cg_predict::run(AnyType &args) {
    using madlib::dbal::eigen_integration::MappedColumnVector;
    MappedColumnVector model = args[0].getAs<MappedColumnVector>();
    MappedColumnVector indVar = args[1].getAs<MappedColumnVector>();

    double p = LinearSVM<MappedColumnVector, GLMTuple>::predict(model, indVar);

    return (p > 0.);
}

} // namespace convex

} // namespace modules

} // namespace madlib

