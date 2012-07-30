/* ----------------------------------------------------------------------- *//**
 *
 * @file lmf.hpp
 *
 * This file contains objective function related computation, which is called
 * by classes in algo/, e.g.,  loss, gradient functions
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TASK_LMF_HPP_
#define MADLIB_MODULES_CONVEX_TASK_LMF_HPP_

#include <dbconnector/dbconnector.hpp>

namespace madlib {

namespace modules {

namespace convex {

// Use Eigen
using namespace madlib::dbal::eigen_integration;

template <class Model, class Tuple>
class LMF {
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
LMF<Model, Tuple>::gradient(
        const model_type                    &model,
        const independent_variables_type    &x,
        const dependent_variable_type       &y,
        model_type                          &gradient) {
    throw std::runtime_error("Not implemented: LMF is good for sparse only.");
}

template <class Model, class Tuple>
void
LMF<Model, Tuple>::gradientInPlace(
        model_type                          &model,
        const independent_variables_type    &x, 
        const dependent_variable_type       &y, 
        const double                        &stepsize) {
    // Please refer to the design document for an explanation of the following
    double e = model.matrixU.row(x.i) * trans(model.matrixV.row(x.j)) - y;
    RowVector temp = model.matrixU.row(x.i)
        - stepsize * e * model.matrixV.row(x.j);
    model.matrixV.row(x.j) -= stepsize * e * model.matrixU.row(x.i);
    model.matrixU.row(x.i) = temp;
}

template <class Model, class Tuple>
double 
LMF<Model, Tuple>::loss(
        const model_type                    &model, 
        const independent_variables_type    &x, 
        const dependent_variable_type       &y) {
    // HAYING: A chance for improvement by reusing the e computed in gradient.
    // Perhaps we can add a book-keeping data structure in the model...
    // Note: the value of e is different from the e in gradient if the
    // model passed in is different, which IS the case for IGD
    // Note 2: this can actually be a problem of having the computation 
    // around model (algo/ & task/) detached from the model classes 
    double e = model.matrixU.row(x.i) * trans(model.matrixV.row(x.j)) - y;
    return e * e;
}

// Not currently used.
template <class Model, class Tuple>
typename Tuple::dependent_variable_type
LMF<Model, Tuple>::predict(
        const model_type                    &model, 
        const independent_variables_type    &x) {
    return model.matrixU.row(x.i) * trasn(model.matrixV.row(x.j));
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

