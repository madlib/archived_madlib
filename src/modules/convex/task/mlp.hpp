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
    
    static dependent_variable_type predict(
            const model_type                    &model, 
            const independent_variables_type    &x);

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
typename Tuple::dependent_variable_type
MLP<Model, Tuple>::predict(
        const model_type                    &model, 
        const independent_variables_type    &x) {
    (void) model;
    (void) x;
    return 0.;
}

template <class Model, class Tuple>
void
MLP<Model, Tuple>::feedForward(
        const model_type                    &model,
        const independent_variables_type    &y,
        std::vector<ColumnVector>           &net,
        std::vector<ColumnVector>           &x) {
    (void) model;
    (void) y;
    (void) net;
    (void) x;
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

