/* ----------------------------------------------------------------------- *//**
 *
 * @file lmf_igd.cpp
 *
 * @brief Low-rank Matrix Factorization functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>

#include "lmf_igd.hpp"

#include "task/lmf.hpp"
#include "algo/igd.hpp"
#include "algo/loss.hpp"

#include "type/tuple.hpp"
#include "type/model.hpp"
#include "type/state.hpp"

namespace madlib {

namespace modules {

namespace convex {

// This 2 classes contain public static methods that can be called
typedef IGD<LMFIGDState<MutableArrayHandle<double> >, LMFIGDState<ArrayHandle<double> >,
        LMF<LMFModel<MutableArrayHandle<double> >, LMFTuple > > LMFIGDAlgorithm;

typedef Loss<LMFIGDState<MutableArrayHandle<double> >, LMFIGDState<ArrayHandle<double> >,
        LMF<LMFModel<MutableArrayHandle<double> >, LMFTuple > > LMFLossAlgorithm;

/**
 * @brief Perform the low-rank matrix factorization transition step
 *
 * Called for each tuple.
 */
AnyType
lmf_igd_transition::run(AnyType &args) {
    // The real state.
    // For the first tuple: args[0] is nothing more than a marker that
    // indicates that we should do some initial operations.
    // For other tuples: args[0] holds the computation state until last tuple
    LMFIGDState<MutableArrayHandle<double> > state = args[0];

    // initilize the state if first tuple
    if (state.algo.numRows == 0) {
        if (!args[4].isNull()) {
            LMFIGDState<ArrayHandle<double> > previousState = args[4];
            state.allocate(*this, previousState.task.rowDim,
                    previousState.task.colDim, previousState.task.maxRank);
            state = previousState;
        } else {
            // configuration parameters
            uint16_t rowDim = args[5].getAs<uint16_t>();
            if (rowDim == 0) {
                throw std::runtime_error("Invalid parameter: row_dim = 0");
            }
            uint16_t columnDim = args[6].getAs<uint16_t>();
            if (columnDim == 0) {
                throw std::runtime_error("Invalid parameter: column_dim = 0");
            }
            uint16_t maxRank = args[7].getAs<uint16_t>();
            if (maxRank == 0) {
                throw std::runtime_error("Invalid parameter: max_rank = 0");
            }
            double stepsize = args[8].getAs<double>();
            if (stepsize <= 0.) {
                throw std::runtime_error("Invalid parameter: stepsize <= 0.0");
            }
            double scaleFactor = args[9].getAs<double>();
            if (scaleFactor <= 0.) {
                throw std::runtime_error("Invalid parameter: scale_factor <= "
                        "0.0");
            }

            state.allocate(*this, rowDim, columnDim, maxRank);
            state.task.stepsize = stepsize;
            state.task.scaleFactor = scaleFactor;
            state.task.model.initialize(scaleFactor);
        }
        // resetting in either case
        state.reset();
    }

    // tuple
    LMFTuple tuple;
    tuple.indVar.i = args[1].getAs<uint16_t>();
    tuple.indVar.j = args[2].getAs<uint16_t>();
    if (tuple.indVar.i == 0 || tuple.indVar.j == 0) {
        throw std::runtime_error("Invalid parameter: [col_row] = 0 or "
                "[col_column] = 0 in table [rel_source]");
    }
    // database starts from 1, while C++ starts from 0
    tuple.indVar.i --;
    tuple.indVar.j --;
    tuple.depVar = args[3].getAs<double>();

    // Now do the transition step
    LMFIGDAlgorithm::transition(state, tuple);
    LMFLossAlgorithm::transition(state, tuple);
    state.algo.numRows ++;

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
lmf_igd_merge::run(AnyType &args) {
    LMFIGDState<MutableArrayHandle<double> > stateLeft = args[0];
    LMFIGDState<ArrayHandle<double> > stateRight = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.algo.numRows == 0) { return stateRight; }
    else if (stateRight.algo.numRows == 0) { return stateLeft; }

    // Merge states together
    LMFIGDAlgorithm::merge(stateLeft, stateRight);
    LMFLossAlgorithm::merge(stateLeft, stateRight);
    // The following numRows update, cannot be put above, because the model
    // averaging depends on their original values
    stateLeft.algo.numRows += stateRight.algo.numRows;

    return stateLeft;
}

/**
 * @brief Perform the low-rank matrix factorization final step
 */
AnyType
lmf_igd_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    LMFIGDState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.algo.numRows == 0) { return Null(); }

    // finalizing
    LMFIGDAlgorithm::final(state);
    // LMFLossAlgorithm::final(state); // empty function call causes a warning
    state.computeRMSE();

    // for stepsize tuning
    dberr << "RMSE: " << state.task.RMSE << std::endl;

    return state;
}

/**
 * @brief Return the difference in RMSE between two states
 */
AnyType
internal_lmf_igd_distance::run(AnyType &args) {
    LMFIGDState<ArrayHandle<double> > stateLeft = args[0];
    LMFIGDState<ArrayHandle<double> > stateRight = args[1];

    return std::abs(stateLeft.task.RMSE - stateRight.task.RMSE);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_lmf_igd_result::run(AnyType &args) {
    LMFIGDState<ArrayHandle<double> > state = args[0];

    Matrix U = trans(state.task.model.matrixU);
    Matrix V = trans(state.task.model.matrixV);
    double RMSE = state.task.RMSE;

    AnyType tuple;
    tuple << U << V << RMSE;

    return tuple;
}

} // namespace convex

} // namespace modules

} // namespace madlib

