/* ----------------------------------------------------------------------- *//**
 *
 * @file mlp.hpp
 *
 * This file contains objective function related computation, which is called
 * by classes in algo/, e.g.,  loss, gradient functions
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TASK_MLP_HPP_
#define MADLIB_MODULES_CONVEX_TASK_MLP_HPP_

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

    static void gradient(
            const model_type                    &model,
            const independent_variables_type    &y,
            const dependent_variable_type       &z, 
            model_type                          &gradient);

    static void gradientInPlace(
            model_type                          &model, 
            const independent_variables_type    &y,
            const dependent_variable_type       &z, 
            const double                        &stepsize);
    
    static double loss(
            const model_type                    &model, 
            const independent_variables_type    &y, 
            const dependent_variable_type       &z);
    
    static void predict(
            const model_type                    &model, 
            const independent_variables_type    &y,
            dependent_variable_type             &x_N);

private:
    static double logistic(const double         &xi) {
        return 1. / (1. + std::exp(-xi));
    }

    static double logisticDerivative(
            const double                        &xi) {
        double value = logistic(xi);
        return value * (1. - value);
    }

    static void feedForward(
            const model_type                    &model,
            const independent_variables_type    &y,
            std::vector<ColumnVector>           &net,
            std::vector<ColumnVector>           &x);

    static void endLayerDeltaError(
            const std::vector<ColumnVector>     &net,
            const std::vector<ColumnVector>     &x,
            const dependent_variable_type       &z,
            ColumnVector                        &delta_N);

    static void errorBackPropagation(
            const ColumnVector                  &delta_N,
            const std::vector<ColumnVector>     &net,
            const model_type                    &model,
            std::vector<ColumnVector>           &delta);
};

template <class Model, class Tuple>
void
MLP<Model, Tuple>::gradient(
        const model_type                    &model,
        const independent_variables_type    &y,
        const dependent_variable_type       &z,
        model_type                          &gradient) {
    (void) model;
    (void) z;
    (void) y;
    (void) gradient;
}

template <class Model, class Tuple>
void
MLP<Model, Tuple>::gradientInPlace(
        model_type                          &model,
        const independent_variables_type    &y,
        const dependent_variable_type       &z,
        const double                        &stepsize) {
    (void) model;
    (void) z;
    (void) y;
    (void) stepsize;
}

template <class Model, class Tuple>
double 
MLP<Model, Tuple>::loss(
        const model_type                    &model, 
        const independent_variables_type    &y, 
        const dependent_variable_type       &z) {
    (void) model;
    (void) z;
    (void) y;
    return 0.;
}

template <class Model, class Tuple>
void
MLP<Model, Tuple>::predict(
        const model_type                    &model, 
        const independent_variables_type    &y,
        dependent_variable_type             &x_N) {
    (void) model;
    (void) y;
    (void) x_N;
    return 0.;
}

template <class Model, class Tuple>
void
MLP<Model, Tuple>::feedForward(
        const model_type                    &model,
        const independent_variables_type    &y,
        std::vector<ColumnVector>           &net,
        std::vector<ColumnVector>           &x) {
    // meta data and x_k^0 = 1
    uint16_t k, j, s;
    uint16_t N = model.u.size(); // assuming >= 1
    net.resize(N + 1);
    x.resize(N + 1);

    std::vector<uint16_t> n; n.clear();
    n.push_back(model.u[0].rows());
    x[0].resize(n[0] + 1);
    x[0](0) = 1.;
    for (k = 1; k <= N; k ++) {
        n.push_back(model.u[k-1].cols());
        net[k].resize(n[k] + 1);
        x[k].resize(n[k] + 1);
        x[k](0) = 1.;
    }

    // y is a mapped parameter from DB, aligning with x here
    for (j = 1; j <= n[0]; j ++) { x[0](j) = y(j-1); }

    for (k = 1; k <= N; k ++) {
        for (j = 1; j <= n[k]; j ++) {
            net[k](j) = 0.;
            for (s = 0; s <= n[k-1]; s ++) {
                net[k](j) += x[k-1] * model.u[k-1](s, j);
            }
            x[k](j) = logistic(net[k](j));
        }
    }
}

template <class Model, class Tuple>
void
MLP<Model, Tuple>::endLayerDeltaError(
        const std::vector<ColumnVector>     &net,
        const std::vector<ColumnVector>     &x,
        const dependent_variable_type       &z,
        ColumnVector                        &delta_N) {
    //meta data
    uint16_t t;
    uint16_t N = x.size() - 1; // assuming >= 1
    uint16_t n_N = x[N].cols() - 1;
    delta_N.resize(n_N + 1);

    for (t = 1; t <= n_N; t ++) {
        delta_N(t) = (x[N](t) - z(t-1)) * logisticDerivative(net[N](t));
    }
}

template <class Model, class Tuple>
void
MLP<Model, Tuple>::errorBackPropagation(
        const ColumnVector                  &delta_N,
        const std::vector<ColumnVector>     &net,
        const model_type                    &model,
        std::vector<ColumnVector>           &delta) {
    // meta data
    uint16_t k, j, t;
    uint16_t N = model.u.size(); // assuming >= 1
    delta.resize(N + 1);
    std::vector<uint16_t> n; n.clear();
    n.push_back(model.u[0].rows());
    for (k = 1; k <= N; k ++) {
        n.push_back(model.u[k-1].cols());
        delta[k].resize(n[k]);
    }
    delta[N] = delta_N;

    for (k = N - 1; k >= 1; k --) {
        for (j = 1; j < n[k+1]; j ++) {
            delta[k](j) = 0.;
            for (t = 1; t <= n[k+1]; t ++) {
                delta[k](j) += delta[k+1](t) * model.u[k](j, t);
            }
            delta[k](j) = delta[k](j) * logisticDerivative(net[k](j));
        }
    }
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

