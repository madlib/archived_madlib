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

    // initialize the state if first tuple
    if (state.algo.numRows == 0) {
        LinearSVM<GLMModel, GLMTuple >::epsilon = args[9].getAs<double>();;
        LinearSVM<GLMModel, GLMTuple >::is_svc = args[10].getAs<bool>();;
        if (!args[3].isNull()) {
            GLMIGDState<ArrayHandle<double> > previousState = args[3];
            state.allocate(*this, previousState.task.dimension);
            state = previousState;
        } else {
            // configuration parameters
            uint32_t dimension = args[4].getAs<uint32_t>();
            state.allocate(*this, dimension); // with zeros
        }
        // resetting in either case
        state.reset();
        state.task.stepsize = args[5].getAs<double>();
        const double lambda = args[6].getAs<double>();
        const bool isL2 = args[7].getAs<bool>();
        const int nTuples = args[8].getAs<int>();
        L1<GLMModel>::n_tuples = nTuples;
        L2<GLMModel>::n_tuples = nTuples;
        if (isL2)
            L2<GLMModel>::lambda = lambda;
        else
            L1<GLMModel>::lambda = lambda;
    }

    // Skip the current record if args[1] (features) contains NULL values,
    // or args[2] is NULL
    try {
        args[1].getAs<MappedColumnVector>();
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }
    if (args[2].isNull())
        return args[0];

    // tuple
    using madlib::dbal::eigen_integration::MappedColumnVector;
    GLMTuple tuple;

    // each tuple can be weighted - this can be combination of the sample weight
    // and the class weight. Calling function is responsible for combining the two
    // into a single tuple weight. The default value for this parameter should be 1.
    const double tuple_weight = args[11].getAs<double>();

    tuple.indVar.rebind(args[1].getAs<MappedColumnVector>().memoryHandle(),
                        state.task.dimension);

    // tuple weight is multiplied to the gradient update. That is equivalent to
    // multiplying with the dependent variable
    tuple.depVar = args[2].getAs<double>() * tuple_weight;

    // Now do the transition step
    // apply IGD with regularization
    L2<GLMModel>::scaling(state.algo.incrModel, state.task.stepsize);
    LinearSVMIGDAlgorithm::transition(state, tuple);
    L1<GLMModel>::clipping(state.algo.incrModel, state.task.stepsize);
    // evaluate objective function and its gradient
    // at the old model - state.task.model
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

    state.algo.loss += L2<GLMModel>::loss(state.task.model);
    state.algo.loss += L1<GLMModel>::loss(state.task.model);
    L2<GLMModel>::gradient(state.task.model, state.algo.gradient);
    L1<GLMModel>::gradient(state.task.model, state.algo.gradient);

    // finalizing
    LinearSVMIGDAlgorithm::final(state);
    elog(NOTICE, "loss = %e, |gradient| = %e, |model| = %e\n",
         (double) state.algo.loss,
         state.algo.gradient.norm(),
         state.task.model.norm());
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
            / stateLeft.algo.loss);
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
