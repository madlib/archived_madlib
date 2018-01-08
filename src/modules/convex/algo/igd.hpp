/* ----------------------------------------------------------------------- *//**
 *
 * @file igd.hpp
 *
 * Generic implementaion of incremental gradient descent, in the fashion of
 * user-definied aggregates. They should be called by actually database
 * functions, after arguments are properly parsed.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#ifndef MADLIB_MODULES_CONVEX_ALGO_IGD_HPP_
#define MADLIB_MODULES_CONVEX_ALGO_IGD_HPP_

namespace madlib {

namespace modules {

namespace convex {

// use Eigen
using namespace madlib::dbal::eigen_integration;

// The reason for using ConstState instead of const State to reduce the
// template type list: flexibility to high-level for mutability control
// More: cast<ConstState>(MutableState) may not always work
template <class State, class ConstState, class Task>
class IGD {
public:
    typedef State state_type;
    typedef ConstState const_state_type;
    typedef typename Task::tuple_type tuple_type;
    typedef typename Task::model_type model_type;

    static void transition(state_type &state, const tuple_type &tuple);
    static void transitionInMiniBatch(state_type &state, const tuple_type &tuple);
    static void transitionInMiniBatch2(state_type &state, const tuple_type &tuple);
    static void merge(state_type &state, const_state_type &otherState);
    static void mergeInPlace(state_type &state, const_state_type &otherState);
    static void final(state_type &state);
};

template <class State, class ConstState, class Task>
void
IGD<State, ConstState, Task>::transition(state_type &state,
        const tuple_type &tuple) {
    // The reason for update model inside a Task:: function instead of
    // returning the gradient and do it here: the gradient is a sparse
    // representation of the model (which is dense), returning the gradient
    // forces the algo to be aware of one more template type
    // -- Task::sparse_model_type, which we do not explicit define

    // apply to the model directly
    Task::gradientInPlace(
            state.algo.incrModel,
            tuple.indVar,
            tuple.depVar,
            state.task.stepsize * tuple.weight);
}

/**
  * @brief Update the transition state in mini-batches
  *
  * Note: We assume that
  *     1. Task defines a model_eigen_type
  *     2. A batch of tuple.indVar is a Matrix
  *     3. A batch of tuple.depVar is a ColumnVector
  *     4. Task defines a lossAndGradient method
  *
 */
 template <class State, class ConstState, class Task>
 void
 IGD<State, ConstState, Task>::transitionInMiniBatch(state_type &state,
         const tuple_type &tuple) {

     typedef typename Task::model_eigen_type model_eigen_type;

     int batch_size = state.algo.batchSize;
     int n_epochs = state.algo.nEpochs;
     int N = tuple.indVar.rows();
     int d = tuple.indVar.cols();
     int n_batches = N < batch_size ? 1 : N / batch_size + int(N%batch_size > 0);

     // elog(INFO, "Number of items = %d, batch_size = %d, Number of batches = %d\n", N, batch_size, n_batches);

     model_eigen_type gradient(state.task.model);
     double avg_loss = 0.0;
     for (int i=0; i < n_epochs; i++) {
         double loss = 0.0;
         for (int j=0, k=0; j < n_batches; j++, k += batch_size) {
             Matrix X_batch;
             ColumnVector y_batch;
             if (j == n_batches-1) {
                // last batch
                X_batch = tuple.indVar.bottomRows(N-k);
                y_batch = tuple.depVar.tail(N-k);
             } else {
                 X_batch = tuple.indVar.block(k, 0, batch_size, d);
                 y_batch = tuple.depVar.segment(k, batch_size);
             }
             loss += Task::lossAndGradient(state.task.model,
                                           X_batch, y_batch,
                                           gradient);
             state.task.model -= state.task.stepsize * (gradient + state.task.reg * state.task.model);
         }
         loss /= n_batches;
         // elog(NOTICE, "Epoch %d, loss = %e\n", i, loss);
         // return average loss for the first epoch
         if (i==0) state.algo.loss += loss;
     }
     return;
 }

/**
  * @brief Update the transition state in mini-batches
  *
  * Note: We assume that
  *     1. Task defines a model_eigen_type
  *     2. A batch of tuple.indVar is a Matrix
  *     3. A batch of tuple.depVar is a ColumnVector
  *     4. Task defines a lossAndUpdateModel method
  *
 */
 template <class State, class ConstState, class Task>
 void
 IGD<State, ConstState, Task>::transitionInMiniBatch2(
        state_type &state,
        const tuple_type &tuple) {

    int batch_size = state.algo.batchSize;
    int n_epochs = state.algo.nEpochs;
    int N = tuple.indVar.rows();
    int d = tuple.indVar.cols();
    int n_batches = N < batch_size ? 1 : N / batch_size + int(N%batch_size > 0);
    // elog(INFO, "Number of items = %d, batch_size = %d, Number of batches = %d\n", N, batch_size, n_batches);
    for (int i=0; i < n_epochs; i++) {
        double loss = 0.0;
        for (int j=0, k=0; j < n_batches; j++, k += batch_size) {
           Matrix X_batch;
           ColumnVector y_batch;
           if (j == n_batches-1) {
              // last batch
              X_batch = tuple.indVar.bottomRows(N-k);
              y_batch = tuple.depVar.tail(N-k);
           } else {
               X_batch = tuple.indVar.block(k, 0, batch_size, d);
               y_batch = tuple.depVar.segment(k, batch_size);
           }
           loss += Task::getLossAndUpdateModel(
               state.task.model, X_batch, y_batch, state.task.stepsize);
           // state.task.model -= state.task.stepsize * (gradient + state.task.reg * state.task.model);
        }
        // elog(NOTICE, "Epoch %d, loss = %e\n", i, loss);
        // return average loss for the first epoch
        if (i==0) state.algo.loss += loss;
    }
    return;
 }


template <class State, class ConstState, class Task>
void
IGD<State, ConstState, Task>::merge(state_type &state,
        const_state_type &otherState) {
    // Having zero checking here to reduce dependency to the caller.
    // This can be removed if it affects performance in the future,
    // with the expectation that callers should do the zero checking.
    if (state.algo.numRows == 0) {
        state.algo.incrModel = otherState.algo.incrModel;
        return;
    } else if (otherState.algo.numRows == 0) {
        return;
    }

    // The reason of this weird algorithm instead of an intuitive one
    // -- (w1 * m1 + w2 * m2) / (w1 + w2): we have only one mutable state,
    // therefore, (m1 * w1 / w2  + m2)  * w2 / (w1 + w2).
    // Order:         111111111  22222  3333333333333333

    // model averaging, weighted by rows seen
    double totalNumRows = static_cast<double>(state.algo.numRows + otherState.algo.numRows);
    state.algo.incrModel *= static_cast<double>(state.algo.numRows) /
        static_cast<double>(otherState.algo.numRows);
    state.algo.incrModel += otherState.algo.incrModel;
    state.algo.incrModel *= static_cast<double>(otherState.algo.numRows) /
        static_cast<double>(totalNumRows);
}

template <class State, class ConstState, class Task>
void
IGD<State, ConstState, Task>::mergeInPlace(state_type &state,
        const_state_type &otherState) {
    // avoid division by zero
    if (state.algo.numRows == 0) {
        state.task.model = otherState.task.model;
        return;
    } else if (otherState.algo.numRows == 0) {
        return;
    }

    // model averaging, weighted by rows seen
    double leftRows = static_cast<double>(state.algo.numRows + state.algo.numBuffers);
    double rightRows = static_cast<double>(otherState.algo.numRows + otherState.algo.numBuffers);
    double totalNumRows = leftRows + rightRows;
    state.task.model *= leftRows / rightRows;
    state.task.model += otherState.task.model;
    state.task.model *= rightRows / totalNumRows;
}

template <class State, class ConstState, class Task>
void
IGD<State, ConstState, Task>::final(state_type &state) {
    // The reason that we have to keep the task.model untouched in transition
    // funtion: loss computation needs the model from last iteration cleanly
    state.task.model = state.algo.incrModel;

}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

