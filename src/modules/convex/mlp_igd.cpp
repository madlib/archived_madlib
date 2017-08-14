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
#include "task/l2.hpp"
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

typedef MLPModel<MutableArrayHandle<double> > MLPModelType;

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
            int numberOfStages = numbersOfUnits.size() - 1;

            double stepsize = args[5].getAs<double>();

            state.allocate(*this, numberOfStages,
                           reinterpret_cast<const double *>(numbersOfUnits.ptr()));
            state.task.stepsize = stepsize;


            const int activation = args[6].getAs<int>();
            const int is_classification = args[7].getAs<int>();

            const bool warm_start = args[9].getAs<bool>();
            const int n_tuples = args[11].getAs<int>();
            const double lambda = args[12].getAs<double>();
            state.task.lambda = lambda;
            MLPTask::lambda = lambda;
            double is_classification_double = (double) is_classification;
            double activation_double = (double) activation;
            MappedColumnVector coeff = args[10].getAs<MappedColumnVector>();
            state.task.model.rebind(&is_classification_double,&activation_double,
                                    &coeff.data()[0], numberOfStages,
                                    &numbersOfUnits[0]);
        }
        // resetting in either case
        state.reset();
    }

    // meta data
    const uint16_t N = state.task.numberOfStages;
    const double *n = state.task.numbersOfUnits;

    MappedColumnVector x_means = args[13].getAs<MappedColumnVector>();
    MappedColumnVector x_stds = args[14].getAs<MappedColumnVector>();
    // tuple
    ColumnVector indVar;
    MappedColumnVector depVar;
    try {
        indVar = (args[1].getAs<MappedColumnVector>()-x_means).cwiseQuotient(x_stds);
        MappedColumnVector y = args[2].getAs<MappedColumnVector>();
        depVar.rebind(y.memoryHandle(), y.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }
    MLPTuple tuple;
    tuple.indVar = indVar;
    tuple.depVar.rebind(depVar.memoryHandle(), depVar.size());
    tuple.weight = args[8].getAs<double>();

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

    if (stateLeft.algo.numRows == 0) { return stateRight; }
    else if (stateRight.algo.numRows == 0) { return stateLeft; }

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

    if (state.algo.numRows == 0) { return Null(); }

    L2<MLPModelType>::lambda = state.task.lambda;
    state.algo.loss = state.algo.loss/static_cast<double>(state.algo.numRows);
    state.algo.loss += L2<MLPModelType>::loss(state.task.model);
    MLPIGDAlgorithm::final(state);

    AnyType tuple;
    tuple << state
          << (double)state.algo.loss;
    return tuple;
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
                    state.task.numbersOfUnits));
    double loss = state.algo.loss;

    AnyType tuple;
    tuple << flattenU
          << loss;
    return tuple;
}

AnyType
internal_predict_mlp::run(AnyType &args) {
    MLPModel<MutableArrayHandle<double> > model;
    ColumnVector indVar;
    int is_response = args[5].getAs<int>();
    MappedColumnVector x_means = args[6].getAs<MappedColumnVector>();
    MappedColumnVector x_stds = args[7].getAs<MappedColumnVector>();
    MappedColumnVector coeff = args[0].getAs<MappedColumnVector>();
    MappedColumnVector layerSizes = args[4].getAs<MappedColumnVector>();
    // Input layer doesn't count
    size_t numberOfStages = layerSizes.size()-1;
    double is_classification = args[2].getAs<double>();
    double activation = args[3].getAs<double>();
    bool get_class = is_classification && is_response;

    model.rebind(&is_classification,&activation,&coeff.data()[0],numberOfStages,&layerSizes.data()[0]);
    try {
        indVar = (args[1].getAs<MappedColumnVector>()-x_means).cwiseQuotient(x_stds);
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }
    ColumnVector prediction = MLPTask::predict(model, indVar, get_class);
    return prediction;
}


} // namespace convex

} // namespace modules

} // namespace madlib

