/* ----------------------------------------------------------------------- *//**
 *
 * @file linear_svm_igd.cpp
 *
 * @brief Linear Support Vector Machine functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#include "linear_svm_igd.hpp"

#include "task/linear_svm.hpp"
#include "algo/igd.hpp"
#include "algo/loss.hpp"

#include "type/tuple.hpp"
#include "type/model.hpp"
#include "type/state.hpp"

namespace madlib {

namespace modules {

namespace convex {

// This 2 classes contain public static methods that can be called
typedef IGD<GLMIGDState<MutableArrayHandle<double> >, GLMIGDState<ArrayHandle<double> >,
        LinearSVM<GLMModel, GLMTuple > > LinearSVMIGDAlgorithm;

typedef Loss<GLMIGDState<MutableArrayHandle<double> >, GLMIGDState<ArrayHandle<double> >,
        LinearSVM<GLMModel, GLMTuple > > LinearSVMLossAlgorithm;

/**
 * @brief Perform the linear support vector machine transition step
 *
 * Called for each tuple.
 */
AnyType
linear_svm_igd_transition::run(AnyType &args) {
    // The real state.
    // For the first tuple: args[0] is nothing more than a marker that
    // indicates that we should do some initial operations.
    // For other tuples: args[0] holds the computation state until last tuple
    GLMIGDState<MutableArrayHandle<double> > state = args[0];

    // initilize the state if first tuple
    if (state.algo.numRows == 0) {
        if (!args[3].isNull()) {
            GLMIGDState<ArrayHandle<double> > previousState = args[3];
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
    tuple.indVar.rebind(args[1].getAs<MappedColumnVector>().memoryHandle(),
            state.task.dimension);
    tuple.depVar = args[2].getAs<bool>() ? 1. : -1.;

    // Now do the transition step
    LinearSVMIGDAlgorithm::transition(state, tuple);
    // regularization, it can be added thru convex framework
    double reg = args[6].getAs<double>();
    if (reg > 0.) {
        for (int i = 0; i < state.algo.incrModel.size(); i ++) {
            if (state.algo.incrModel(i) > reg) {
                state.algo.incrModel(i) -= reg;
            } else if (state.algo.incrModel(i) < -reg) {
                state.algo.incrModel(i) += reg;
            } else { state.algo.incrModel(i) = 0.; }
        }
    }
    LinearSVMLossAlgorithm::transition(state, tuple);
    state.algo.numRows ++;

    // dberr << "incrModel: " << state.algo.incrModel << std::endl;

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
linear_svm_igd_merge::run(AnyType &args) {
    GLMIGDState<MutableArrayHandle<double> > stateLeft = args[0];
    GLMIGDState<ArrayHandle<double> > stateRight = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.algo.numRows == 0) { return stateRight; }
    else if (stateRight.algo.numRows == 0) { return stateLeft; }

    // Merge states together
    LinearSVMIGDAlgorithm::merge(stateLeft, stateRight);
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
linear_svm_igd_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    GLMIGDState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.algo.numRows == 0) { return Null(); }

    // finalizing
    LinearSVMIGDAlgorithm::final(state);
    // LinearSVMLossAlgorithm::final(state); // empty function call causes a warning

    // dberr << "loss: " << state.algo.loss << std::endl;

    return state;
}

/**
 * @brief Return the difference in RMSE between two states
 */
AnyType
internal_linear_svm_igd_distance::run(AnyType &args) {
    GLMIGDState<ArrayHandle<double> > stateLeft = args[0];
    GLMIGDState<ArrayHandle<double> > stateRight = args[1];

    return std::abs((stateLeft.algo.loss - stateRight.algo.loss)
            / stateRight.algo.loss);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_linear_svm_igd_result::run(AnyType &args) {
    GLMIGDState<ArrayHandle<double> > state = args[0];

    AnyType tuple;
    tuple << state.task.model
        << static_cast<double>(state.algo.loss);

    return tuple;
}

/**
 * @brief Return the prediction reselt
 */
AnyType
linear_svm_igd_predict::run(AnyType &args) {
    using madlib::dbal::eigen_integration::MappedColumnVector;
    MappedColumnVector model = args[0].getAs<MappedColumnVector>();
    MappedColumnVector indVar = args[1].getAs<MappedColumnVector>();

    if (model.size() != indVar.size()) {
        throw std::runtime_error(
                "SVM error: sizes of model and provided data do not match!");
    }

    double p = LinearSVM<MappedColumnVector, GLMTuple>::predict(model, indVar);

    return p > 0. ? 1. : -1.;
}

} // namespace convex

} // namespace modules

} // namespace madlib
