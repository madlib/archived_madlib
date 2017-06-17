/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 *
 * @file mlp_igd.cpp
 *
 * @brief Multilayer Perceptron functions
 *
 *//* ----------------------------------------------------------------------- */
#include <boost/lexical_cast.hpp>

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>

#include "mlp_igd.hpp"

#include "task/mlp.hpp"
#include "algo/igd.hpp"
#include "algo/loss.hpp"

#include "type/tuple.hpp"
#include "type/model.hpp"
#include "type/state.hpp"

namespace madlib {

namespace modules {

namespace convex {

// These 2 classes contain public static methods that can be called
typedef IGD<MLPIGDState<MutableArrayHandle<double> >, MLPIGDState<ArrayHandle<double> >,
        MLP<MLPModel<MutableArrayHandle<double> >, MLPTuple > > MLPIGDAlgorithm;

typedef Loss<MLPIGDState<MutableArrayHandle<double> >, MLPIGDState<ArrayHandle<double> >,
        MLP<MLPModel<MutableArrayHandle<double> >, MLPTuple > > MLPLossAlgorithm;

typedef MLP<MLPModel<MutableArrayHandle<double> >,MLPTuple> MLPTask;

/**
 * @brief Perform the multilayer perceptron transition step
 *
 * Called for each tuple.
 */
AnyType
mlp_igd_transition::run(AnyType &args) {
    // For the first tuple: args[0] is nothing more than a marker that
    // indicates that we should do some initial operations.
    // For other tuples: args[0] holds the computation state until last tuple
    MLPIGDState<MutableArrayHandle<double> > state = args[0];

    // initilize the state if first tuple
    if (state.algo.numRows == 0) {
        if (!args[3].isNull()) {
            MLPIGDState<ArrayHandle<double> > previousState = args[3];

            state.allocate(*this, previousState.task.numberOfStages,
                           previousState.task.numbersOfUnits);
            state = previousState;
        } else {
            // configuration parameters
            ArrayHandle<double> numbersOfUnits = args[4].getAs<ArrayHandle<double> >();

            double stepsize = args[5].getAs<double>();

            state.allocate(*this, numbersOfUnits.size() - 1,
                           reinterpret_cast<const double *>(numbersOfUnits.ptr()));
            state.task.stepsize = stepsize;


            int activation = args[6].getAs<int>();

            int is_classification = args[7].getAs<int>();
            state.task.model.initialize(is_classification, activation);
        }

        // resetting in either case
        state.reset();
    }

    // meta data
    const uint16_t N = state.task.numberOfStages;
    const double *n = state.task.numbersOfUnits;

    // tuple
    MappedColumnVector indVar;
    MappedColumnVector depVar;
    try {
        // an exception is raised in the backend if args[2] contains nulls
        MappedColumnVector x = args[1].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        indVar.rebind(x.memoryHandle(), x.size());
        MappedColumnVector y = args[2].getAs<MappedColumnVector>();
        depVar.rebind(y.memoryHandle(), y.size());

    } catch (const ArrayWithNullException &e) {
        return args[0];
    }
    MLPTuple tuple;
    tuple.indVar.rebind(indVar.memoryHandle(), indVar.size());
    tuple.depVar.rebind(depVar.memoryHandle(), depVar.size());

    // Now do the transition step
    MLPIGDAlgorithm::transition(state, tuple);
    MLPLossAlgorithm::transition(state, tuple);
    state.algo.numRows ++;

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
mlp_igd_merge::run(AnyType &args) {
    MLPIGDState<MutableArrayHandle<double> > stateLeft = args[0];
    MLPIGDState<ArrayHandle<double> > stateRight = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.algo.numRows == 0) { return stateRight; }
    else if (stateRight.algo.numRows == 0) { return stateLeft; }

    // Merge states together
    MLPIGDAlgorithm::merge(stateLeft, stateRight);
    MLPLossAlgorithm::merge(stateLeft, stateRight);
    // The following numRows update, cannot be put above, because the model
    // averaging depends on their original values
    stateLeft.algo.numRows += stateRight.algo.numRows;

    return stateLeft;
}

/**
 * @brief Perform the multilayer perceptron final step
 */
AnyType
mlp_igd_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    MLPIGDState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.algo.numRows == 0) { return Null(); }

    // finalizing
    MLPIGDAlgorithm::final(state);

    // Return the mean loss
    state.algo.loss = state.algo.loss/static_cast<double>(state.algo.numRows);

    // for stepsize tuning
    std::stringstream debug;
    debug << "loss: " << state.algo.loss;
    elog(INFO,"%s",debug.str().c_str());
    return state;
}

/**
 * @brief Return the difference in RMSE between two states
 */
AnyType
internal_mlp_igd_distance::run(AnyType &args) {
    MLPIGDState<ArrayHandle<double> > stateLeft = args[0];
    MLPIGDState<ArrayHandle<double> > stateRight = args[1];
    return std::abs(stateLeft.algo.loss - stateRight.algo.loss);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_mlp_igd_result::run(AnyType &args) {
    MLPIGDState<ArrayHandle<double> > state = args[0];

    HandleTraits<ArrayHandle<double> >::ColumnVectorTransparentHandleMap
        flattenU;
    flattenU.rebind(&state.task.model.u[0](0, 0),
            state.task.model.arraySize(state.task.numberOfStages,
                    state.task.numbersOfUnits)-2); // -2 for is_classification and activation
    double loss = state.algo.loss;


    AnyType tuple;
    tuple << flattenU
          << loss;
    return tuple;
}

AnyType
internal_predict_mlp::run(AnyType &args) {
    MLPModel<MutableArrayHandle<double> > model;
    MappedColumnVector indVar;
    int is_response = args[5].getAs<int>();
    MappedColumnVector coeff = args[0].getAs<MappedColumnVector>();
    MappedColumnVector layerSizes = args[4].getAs<MappedColumnVector>();
    // Input layer doesn't count
    size_t numberOfStages = layerSizes.size()-1;
    //#TODO this should be an int not a double
    double is_classification = args[2].getAs<double>();
    double activation = args[3].getAs<double>();
    bool get_class = is_classification && is_response;

    model.rebind(&is_classification,&activation,&coeff.data()[0],numberOfStages,&layerSizes.data()[0]);
    try {
        MappedColumnVector x = args[1].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        indVar.rebind(x.memoryHandle(), x.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }
    ColumnVector prediction = MLPTask::predict(model, indVar, get_class);

    return prediction;
}


} // namespace convex

} // namespace modules

} // namespace madlib

