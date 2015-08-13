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
#include "task/l1.hpp"
#include "task/l2.hpp"
#include "algo/igd.hpp"
#include "algo/loss.hpp"
#include "algo/gradient.hpp"

#include "type/tuple.hpp"
#include "type/model.hpp"
#include "type/state.hpp"

namespace madlib {

namespace modules {

namespace convex {

// This 2 classes contain public static methods that can be called
typedef IGD<GLMIGDState<MutableArrayHandle<double> >,
        GLMIGDState<ArrayHandle<double> >,
        LinearSVM<GLMModel, GLMTuple > > LinearSVMIGDAlgorithm;

typedef Loss<GLMIGDState<MutableArrayHandle<double> >,
        GLMIGDState<ArrayHandle<double> >,
        LinearSVM<GLMModel, GLMTuple > > LinearSVMLossAlgorithm;

typedef Gradient<GLMIGDState<MutableArrayHandle<double> >,
        GLMIGDState<ArrayHandle<double> >,
        LinearSVM<GLMModel, GLMTuple > > LinearSVMGradientAlgorithm;

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

    const double lambda = args[6].getAs<double>();
    const bool isL2 = args[7].getAs<bool>();
    const int nTuples = args[8].getAs<int>();

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
        if (isL2) {
            state.algo.loss += L2<GLMModel>::loss(state.task.model, lambda);
            L2<GLMModel>::gradient(state.task.model, lambda, state.algo.gradient);
        } else {
            state.algo.loss += L1<GLMModel>::loss(state.task.model, lambda);
            L1<GLMModel>::gradient(state.task.model, lambda, state.algo.gradient);
        }
    }

    // Skip the current record if args[1] (features) contains NULL values,
    // or args[2] is NULL
    try {
        args[1].getAs<MappedColumnVector>();
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }
    if (args[2].isNull()) { return args[0]; }

    // tuple
    using madlib::dbal::eigen_integration::MappedColumnVector;
    GLMTuple tuple;
    tuple.indVar.rebind(args[1].getAs<MappedColumnVector>().memoryHandle(),
            state.task.dimension);
    tuple.depVar = args[2].getAs<bool>() ? 1. : -1.;

    // Now do the transition step
    // apply IGD with regularization
    if (isL2) {
        L2<GLMModel>::scaling(state.algo.incrModel, lambda, nTuples, state.task.stepsize);
        LinearSVMIGDAlgorithm::transition(state, tuple);
    } else {
        LinearSVMIGDAlgorithm::transition(state, tuple);
        L1<GLMModel>::clipping(state.algo.incrModel, lambda, nTuples, state.task.stepsize);
    }
    // objective function and its gradient
    LinearSVMLossAlgorithm::transition(state, tuple);
    LinearSVMGradientAlgorithm::transition(state, tuple);

    state.algo.numRows ++;

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
    LinearSVMGradientAlgorithm::merge(stateLeft, stateRight);

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
        << static_cast<double>(state.algo.loss)
        << state.algo.gradient.norm()
        << static_cast<int64_t>(state.algo.numRows);

    return tuple;
}

} // namespace convex

} // namespace modules

} // namespace madlib
