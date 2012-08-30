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
            const independent_variables_type    &x,
            const dependent_variable_type       &y, 
            model_type                          &gradient);

    static void gradientInPlace(
            model_type                          &model, 
            const independent_variables_type    &x,
            const dependent_variable_type       &y, 
            const double                        &stepsize);
    
    static double loss(
            const model_type                    &model, 
            const independent_variables_type    &x, 
            const dependent_variable_type       &y);
    
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
};

template <class Model, class Tuple>
void
MLP<Model, Tuple>::gradient(
        const model_type                    &model,
        const independent_variables_type    &x,
        const dependent_variable_type       &y,
        model_type                          &gradient) {
    (void) model;
    (void) x;
    (void) y;
    (void) gradient;
}

template <class Model, class Tuple>
void
MLP<Model, Tuple>::gradientInPlace(
        model_type                          &model,
        const independent_variables_type    &x, 
        const dependent_variable_type       &y, 
        const double                        &stepsize) {
    (void) model;
    (void) x;
    (void) y;
    (void) stepsize;
}

template <class Model, class Tuple>
double 
MLP<Model, Tuple>::loss(
        const model_type                    &model, 
        const independent_variables_type    &x, 
        const dependent_variable_type       &y) {
    (void) model;
    (void) x;
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

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

