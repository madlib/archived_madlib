/* ----------------------------------------------------------------------- *//**
 *
 * @file lmf.cpp
 *
 * @brief Low-rank Matrix Factorization functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>

#include "lmf.hpp"
#include "task/lmf.hpp"
#include "algo/igd.hpp"
#include "algo/loss.hpp"
#include "type/independent_variables.hpp"
#include "type/dependent_variable.hpp"
#include "type/tuple.hpp"
#include "type/model.hpp"
#include "type/state.hpp"

namespace madlib {

namespace modules {

namespace convex {

typedef IGD<LMFIGDState<MutableArrayHandle<double> >, LMFIGDState<ArrayHandle<double> >,
        LMF<LMFModel<MutableArrayHandle<double> >, LMFTuple > > LMFIGDAlgorithm;
typedef Loss<LMFIGDState<MutableArrayHandle<double> >, LMFIGDState<ArrayHandle<double> >,
        LMF<LMFModel<MutableArrayHandle<double> >, LMFTuple > > LMFLossAlgorithm;

/**
 * @brief Perform the low-rank matrix factorization transition step
 */
AnyType
lmf_igd_transition::run(AnyType &args) {
    // the real state, args[0] is actually not needed
    LMFIGDState<MutableArrayHandle<double> > state = args[0];

    // initilize the state if first row
    if (state.algo.numRows == 0) {
        if (!args[4].isNull()) {
            LMFIGDState<ArrayHandle<double> > previousState = args[4];
            state.allocate(*this, previousState.task.rowDim,
                    previousState.task.colDim, previousState.task.maxRank,
                    previousState.task.stepsize);
            state = previousState;
        } else {
            // configuration parameters
            uint32_t m = args[5].getAs<int32_t>();
            uint32_t n = args[6].getAs<int32_t>();
            uint32_t r = args[7].getAs<int32_t>();
            if (m > std::numeric_limits<uint16_t>::max() ||
                    n > std::numeric_limits<uint16_t>::max() ||
                    r > std::numeric_limits<uint16_t>::max()) {
                throw std::domain_error("m, n, r cannot be larger than 65535");
            }
            uint16_t rowDim = static_cast<uint16_t>(m);
            uint16_t colDim = static_cast<uint16_t>(n);
            uint16_t maxRank = static_cast<uint16_t>(r);
            double stepsize = args[8].getAs<double>();
            double initValue = args[9].getAs<double>();

            state.allocate(*this, rowDim, colDim, maxRank, stepsize);
            state.task.model.initialize(initValue);
        }
        // resetting in either case
        state.reset();
    }

    // tuple
    LMFTuple tuple;
    uint32_t i = args[1].getAs<int32_t>();
    uint32_t j = args[2].getAs<int32_t>();
    tuple.depVar = args[3].getAs<double>();
    if (i > std::numeric_limits<uint16_t>::max() ||
            j > std::numeric_limits<uint16_t>::max()) {
        throw std::domain_error("Indices cannot be larger than 65535.");
    }
    tuple.indVar.i = static_cast<uint16_t>(i);
    tuple.indVar.j = static_cast<uint16_t>(j);

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
    stateLeft.algo.numRows += stateRight.algo.numRows; // cannot be put above

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
    // LMFLossAlgorithm::final(state); // empty function
    state.computeRMSE();
 
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

    AnyType tuple;
    tuple << state.task.model.matrixU
        << state.task.model.matrixV
        << static_cast<double>(state.task.RMSE);

    return tuple;
}

} // namespace convex

} // namespace modules

} // namespace madlib

