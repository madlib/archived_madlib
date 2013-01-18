/* ----------------------------------------------------------------------- *//**
 *
 * @file lasso_igd.cpp
 *
 * @brief LASSO functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#include "lasso_igd.hpp"

#include "task/ols.hpp"
#include "task/l1.hpp"
#include "algo/igd.hpp"
#include "algo/regularized_igd.hpp"
#include "algo/loss.hpp"

#include "type/tuple.hpp"
#include "type/model.hpp"
#include "type/state.hpp"

namespace madlib {

namespace modules {

namespace convex {

// This 4 classes contain public static methods that can be called
typedef L1<GLMModel > GLML1Regularizer;

typedef RegularizedIGD<RegularizedGLMIGDState<MutableArrayHandle<double> >,
        OLS<GLMModel, GLMTuple >,
        GLML1Regularizer > OLSL1RegularizedIGDAlgorithm;

typedef IGD<RegularizedGLMIGDState<MutableArrayHandle<double> >, 
        RegularizedGLMIGDState<ArrayHandle<double> >,
        OLS<GLMModel, GLMTuple > > OLSIGDAlgorithm;

typedef Loss<RegularizedGLMIGDState<MutableArrayHandle<double> >, 
        RegularizedGLMIGDState<ArrayHandle<double> >,
        OLS<GLMModel, GLMTuple > > OLSLossAlgorithm;

/**
 * @brief Perform the LASSO transition step
 *
 * Called for each tuple.
 */
AnyType
lasso_igd_transition::run(AnyType &args) {
    // The real state. 
    // For the first tuple: args[0] is nothing more than a marker that 
    // indicates that we should do some initial operations.
    // For other tuples: args[0] holds the computation state until last tuple
    RegularizedGLMIGDState<MutableArrayHandle<double> > state = args[0];

    // initilize the state if first tuple
    if (state.algo.numRows == 0) {
        if (!args[3].isNull()) {
            RegularizedGLMIGDState<ArrayHandle<double> > previousState = args[3];
            state.allocate(*this, previousState.task.dimension);
            state = previousState;
        } else {
            // configuration parameters
            uint32_t dimension = args[4].getAs<uint32_t>();
            double stepsize = args[5].getAs<double>();
            double lambda = args[6].getAs<double>();
            uint64_t totalRows = args[7].getAs<uint64_t>();

            state.allocate(*this, dimension); // with zeros
            state.task.stepsize = stepsize;
            state.task.lambda = lambda; // regularized_igd.hpp uses lambda / totalRows instead of lambda itself
            state.task.totalRows = totalRows;
        }
        // resetting in either case
        state.reset();
    }

    // tuple
    using madlib::dbal::eigen_integration::MappedColumnVector;
    GLMTuple tuple;
    MappedColumnVector indVar = args[1].getAs<MappedColumnVector>();
    tuple.indVar.rebind(indVar.memoryHandle(), indVar.size());
    tuple.depVar = args[2].getAs<double>();

    // Now do the transition step
    OLSL1RegularizedIGDAlgorithm::transition(state, tuple);
    OLSLossAlgorithm::transition(state, tuple);
    state.algo.numRows ++;

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
lasso_igd_merge::run(AnyType &args) {
    RegularizedGLMIGDState<MutableArrayHandle<double> > stateLeft = args[0];
    RegularizedGLMIGDState<ArrayHandle<double> > stateRight = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.algo.numRows == 0) { return stateRight; }
    else if (stateRight.algo.numRows == 0) { return stateLeft; }

    // Merge states together
    OLSIGDAlgorithm::merge(stateLeft, stateRight);
    OLSLossAlgorithm::merge(stateLeft, stateRight);
    // The following numRows update, cannot be put above, because the model
    // averaging depends on their original values
    stateLeft.algo.numRows += stateRight.algo.numRows;

    return stateLeft;
}

/**
 * @brief Perform the LASSO final step
 */
AnyType
lasso_igd_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    RegularizedGLMIGDState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.algo.numRows == 0) { return Null(); }

    // finalizing
    //GLML1Regularizer::loss(state.task.model, state.task.lambda);
    OLSIGDAlgorithm::final(state);
    
    // for stepsize tuning
    //dberr << "loss: " << state.algo.loss << std::endl;
 
    return state;
}

/**
 * @brief Return the difference in RMSE between two states
 */
AnyType
internal_lasso_igd_distance::run(AnyType &args) {
    RegularizedGLMIGDState<ArrayHandle<double> > stateLeft = args[0];
    RegularizedGLMIGDState<ArrayHandle<double> > stateRight = args[1];

    return std::abs((stateLeft.algo.loss - stateRight.algo.loss)
                    / stateRight.algo.loss);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_lasso_igd_result::run(AnyType &args) {
    RegularizedGLMIGDState<MutableArrayHandle<double> > state = args[0];

    AnyType tuple;
    tuple << state.task.model
          << static_cast<double>(state.algo.loss) +
        (double)(GLML1Regularizer::loss(state.task.model, state.task.lambda));

    return tuple;
}

/**
 * @brief Return the prediction reselt
 */
AnyType
lasso_igd_predict::run(AnyType &args) {
    using madlib::dbal::eigen_integration::MappedColumnVector;
    MappedColumnVector model = args[0].getAs<MappedColumnVector>();

    double intercept = args[1].getAs<double>();
    MappedColumnVector indVar = args[2].getAs<MappedColumnVector>();

    return OLS<MappedColumnVector, GLMTuple>::predict(model, intercept, indVar);
}

} // namespace convex

} // namespace modules

} // namespace madlib

