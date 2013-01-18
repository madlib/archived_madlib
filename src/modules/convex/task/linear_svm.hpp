/* ----------------------------------------------------------------------- *//**
 *
 * @file linear_svm.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TASK_LINEAR_SVM_HPP_
#define MADLIB_MODULES_CONVEX_TASK_LINEAR_SVM_HPP_

#include <dbconnector/dbconnector.hpp>

namespace madlib {

namespace modules {

namespace convex {

// Use Eigen
using namespace madlib::dbal::eigen_integration;

template <class Model, class Tuple>
class LinearSVM {
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
};

template <class Model, class Tuple>
void
LinearSVM<Model, Tuple>::gradient(
        const model_type                    &model,
        const independent_variables_type    &x,
        const dependent_variable_type       &y,
        model_type                          &gradient) {
    double wx = dot(model, x);
    if (1 - wx * y > 0) {
        double c = -y; // minus for "-loglik"
        gradient += c * x;
    } else { }
}

template <class Model, class Tuple>
void
LinearSVM<Model, Tuple>::gradientInPlace(
        model_type                          &model,
        const independent_variables_type    &x, 
        const dependent_variable_type       &y, 
        const double                        &stepsize) {
    double wx = dot(model, x);
    if (1. - wx * y > 0.) {
        double c = -y; // minus for "-loglik"
        model -= stepsize * c * x;
    } else { }
}

template <class Model, class Tuple>
double 
LinearSVM<Model, Tuple>::loss(
        const model_type                    &model, 
        const independent_variables_type    &x, 
        const dependent_variable_type       &y) {
    double wx = dot(model, x);
    double distance = 1. - wx * y;
    return distance > 0. ? distance : 0.;
}

template <class Model, class Tuple>
typename Tuple::dependent_variable_type
LinearSVM<Model, Tuple>::predict(
        const model_type                    &model, 
        const independent_variables_type    &x) {
    return dot(model, x);
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

