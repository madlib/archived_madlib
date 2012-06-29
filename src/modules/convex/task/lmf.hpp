/* ----------------------------------------------------------------------- *//**
 *
 * @file lmf.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_CONVEX_TASK_LMF_H_
#define MADLIB_CONVEX_TASK_LMF_H_

namespace madlib {

namespace modules {

namespace convex {

// Use Eigen
using namespace dbal::eigen_integration;

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
    // HAYING: A chance for improvement by reusing the e computed in gradient
    double e = model.matrixU.row(x.i) * trans(model.matrixV.row(x.j)) - y;
    return e * e;
}

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

