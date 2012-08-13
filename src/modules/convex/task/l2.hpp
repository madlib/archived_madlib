/* ----------------------------------------------------------------------- *//**
 *
 * @file l2.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TASK_L2_HPP_
#define MADLIB_MODULES_CONVEX_TASK_L2_HPP_

#include <dbconnector/dbconnector.hpp>

namespace madlib {

namespace modules {

namespace convex {

// Use Eigen
using namespace madlib::dbal::eigen_integration;

template <class Model, class Hessian = Matrix>
class L2 {
public:
    typedef Model model_type;
    typedef Hessian hessian_type;

    static void gradient(
            const model_type                    &model,
            const double                        &lambda, 
            model_type                          &gradient);

    static void hessian(
            const model_type                    &model,
            const double                        &lambda, 
            hessian_type                        &hessian);

    static double loss(
            const model_type                    &model, 
            const double                        &lambda);
};

template <class Model, class Hessian>
void
L2<Model, Hessian>::gradient(
        const model_type                    &model,
        const double                        &lambda, 
        model_type                          &gradient) {
    gradient += lambda * model;
}

template <class Model, class Hessian>
void
L2<Model, Hessian>::hessian(
        const model_type                    &model,
        const double                        &lambda, 
        hessian_type                        &hessian) {
    hessian += lambda * hessian.Identity(model.rows(), model.rows());
}

template <class Model, class Hessian>
double 
L2<Model, Hessian>::loss(
        const model_type                    &model, 
        const double                        &lambda) {
    return lambda * dot(model, model);
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif


