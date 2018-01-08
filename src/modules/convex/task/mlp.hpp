/* ----------------------------------------------------------------------- *//**
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
 * @file mlp.hpp
 *
 * This file contains objective function related computation, which is called
 * by classes in algo/, e.g.,  loss, gradient functions
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TASK_MLP_HPP_
#define MADLIB_MODULES_CONVEX_TASK_MLP_HPP_

#include <dbconnector/dbconnector.hpp>

namespace madlib {

namespace modules {

namespace convex {

// Use Eigen
using namespace madlib::dbal::eigen_integration;

template <class Model, class Tuple>
class MLP {
public:
    typedef Model model_type;
    typedef Tuple tuple_type;
    typedef typename Tuple::independent_variables_type
        independent_variables_type;
    typedef typename Tuple::dependent_variable_type dependent_variable_type;

    static void gradientInPlace(
            model_type                          &model,
            const independent_variables_type    &x,
            const dependent_variable_type       &y,
            const double                        &stepsize);

    static double getLossAndUpdateModel(
            model_type                          &model,
            const Matrix                        &x,
            const ColumnVector                  &y,
            const double                        &stepsize);

    static double loss(
            const model_type                    &model,
            const independent_variables_type    &x,
            const dependent_variable_type       &y);

    static ColumnVector predict(
            const model_type                    &model,
            const independent_variables_type    &x,
            const bool                          get_class);

    const static int RELU = 0;
    const static int SIGMOID = 1;
    const static int TANH = 2;
    static double lambda;

private:
    static double sigmoid(const double &xi) {
        return 1. / (1. + std::exp(-xi));
    }

    static double relu(const double &xi) {
        return xi*(xi>0);
    }

    static double tanh(const double &xi) {
        return std::tanh(xi);
    }

    static double sigmoidDerivative(const double &xi) {
        double value = sigmoid(xi);
        return value * (1. - value);
    }

    static double reluDerivative(const double &xi) {
        return xi>0;
    }

    static double tanhDerivative(const double &xi) {
        double value = tanh(xi);
        return 1-value*value;
    }

    static void feedForward(
            const model_type                    &model,
            const independent_variables_type    &x,
            std::vector<ColumnVector>           &net,
            std::vector<ColumnVector>           &o);

    static void backPropogate(
            const ColumnVector                  &y_true,
            const ColumnVector                  &y_estimated,
            const std::vector<ColumnVector>     &net,
            const model_type                    &model,
            std::vector<ColumnVector>           &delta);
};

template <class Model, class Tuple>
double MLP<Model, Tuple>::lambda = 0;

template <class Model, class Tuple>
double
MLP<Model, Tuple>::getLossAndUpdateModel(
        model_type           &model,
        const Matrix         &x_batch,
        const ColumnVector   &y_true_batch,
        const double         &stepsize) {

    uint16_t N = model.u.size(); // assuming nu. of layers >= 1
    size_t n = x_batch.rows();
    size_t i, k;
    double total_loss = 0.;

    // gradient added over the batch
    std::vector<Matrix> total_gradient_per_layer(N);
    for (k=0; k < N; ++k)
        total_gradient_per_layer[k] = Matrix::Zero(model.u[k].rows(),
                                                   model.u[k].cols());

    for (i=0; i < n; i++){
        ColumnVector x = x_batch.row(i);
        ColumnVector y_true = y_true_batch.segment(i, 1);
        // FIXME: CURRENTLY HARD-CODED FOR SINGLE OUTPUT NODE

        std::vector<ColumnVector> net, o, delta;
        feedForward(model, x, net, o);
        backPropogate(y_true, o.back(), net, model, delta);

        for (k=0; k < N; k++){
                total_gradient_per_layer[k] += o[k] * delta[k].transpose();
        }

        // loss computation
        ColumnVector y_estimated = o.back();
        total_loss += 0.5 * (y_estimated - y_true).squaredNorm();
    }

    for (k=0; k < N; k++){
        Matrix regularization = MLP<Model, Tuple>::lambda * model.u[k];
        regularization.row(0).setZero(); // Do not update bias
        model.u[k] -= stepsize * (total_gradient_per_layer[k] / n + regularization);
    }
    return total_loss;
}

template <class Model, class Tuple>
void
MLP<Model, Tuple>::gradientInPlace(
        model_type                          &model,
        const independent_variables_type    &x,
        const dependent_variable_type       &y_true,
        const double                        &stepsize) {
    uint16_t N = model.u.size(); // assuming nu. of layers >= 1
    uint16_t k;
    std::vector<ColumnVector> net, o, delta;

    feedForward(model, x, net, o);
    backPropogate(y_true, o.back(), net, model, delta);

    for (k=0; k < N; k++){
        Matrix regularization = MLP<Model, Tuple>::lambda*model.u[k];
        regularization.row(0).setZero(); // Do not update bias
        model.u[k] -= stepsize * (o[k] * delta[k].transpose() + regularization);
    }
}

template <class Model, class Tuple>
double
MLP<Model, Tuple>::loss(
        const model_type                    &model,
        const independent_variables_type    &x,
        const dependent_variable_type       &y_true) {
    // Here we compute the loss. In the case of regression we use sum of square errors
    // In the case of classification the loss term is cross entropy.
    std::vector<ColumnVector> net, o;
    feedForward(model, x, net, o);
    ColumnVector y_estimated = o.back();

    if(model.is_classification){
        double clip = 1.e-10;
        y_estimated = y_estimated.cwiseMax(clip).cwiseMin(1.-clip);
        return - (y_true.array()*y_estimated.array().log()
               + (-y_true.array()+1)*(-y_estimated.array()+1).log()).sum();
    }
    else{
        return 0.5 * (y_estimated - y_true).squaredNorm();
    }
}

template <class Model, class Tuple>
ColumnVector
MLP<Model, Tuple>::predict(
        const model_type                    &model,
        const independent_variables_type    &x,
        const bool                          get_class) {
    std::vector<ColumnVector> net, o;

    feedForward(model, x, net, o);
    ColumnVector output = o.back();

    if(get_class){ // Return a length 1 array with the predicted index
        int max_idx;
        output.maxCoeff(&max_idx);
        output.resize(1);
        output[0] = (double) max_idx;
    }
    return output;
}


template <class Model, class Tuple>
void
MLP<Model, Tuple>::feedForward(
        const model_type                    &model,
        const independent_variables_type    &x,
        std::vector<ColumnVector>           &net,
        std::vector<ColumnVector>           &o){
    uint16_t k, N;
    N = model.u.size(); // assuming >= 1
    net.resize(N + 1);
    o.resize(N + 1);

    double (*activation)(const double&);
    if(model.activation==RELU)
        activation = &relu;
    else if(model.activation==SIGMOID)
        activation = &sigmoid;
    else
        activation = &tanh;

    o[0].resize(x.size() + 1);
    o[0] << 1.,x;

    for (k = 1; k < N; k ++) {
        net[k] = model.u[k-1].transpose() * o[k-1];
        o[k] = ColumnVector(model.u[k-1].cols() + 1);
        o[k] << 1., net[k].unaryExpr(activation);
    }
    o[N] = model.u[N-1].transpose() * o[N-1];

    // Numerically stable calculation of softmax
    if(model.is_classification){
        double max_x = o[N].maxCoeff();
        o[N] = (o[N].array() - max_x).exp();
        // elog(INFO, "o.sum = %d", o[N].sum());
        o[N] /= o[N].sum();
    }
}

template <class Model, class Tuple>
void
MLP<Model, Tuple>::backPropogate(
        const ColumnVector                  &y_true,
        const ColumnVector                  &y_estimated,
        const std::vector<ColumnVector>     &net,
        const model_type                    &model,
        std::vector<ColumnVector>           &delta) {
    uint16_t k, N;
    N = model.u.size(); // assuming >= 1
    delta.resize(N);

    double (*activationDerivative)(const double&);
    if(model.activation==RELU)
        activationDerivative = &reluDerivative;
    else if(model.activation==SIGMOID)
        activationDerivative = &sigmoidDerivative;
    else
        activationDerivative = &tanhDerivative;

    delta.back() = y_estimated - y_true;
    for (k = N - 1; k >= 1; k --) {
        // Do not include the bias terms
        delta[k-1] = model.u[k].bottomRows(model.u[k].rows()-1) * delta[k];
        delta[k-1] = delta[k-1].array() * net[k].unaryExpr(activationDerivative).array();
    }
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

