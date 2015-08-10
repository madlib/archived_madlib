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

    static void scaling(
            model_type                          &incrModel,
            const double                        &lambda,
            const int                           &n_tuples,
            const double                        &stepsize);

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
    gradient += 2 * lambda * model;
}

template <class Model, class Hessian>
void
L2<Model, Hessian>::scaling(
        model_type                          &incrModel,
        const double                        &lambda,
        const int                           &n_tuples,
        const double                        &stepsize) {
    double wscale = 1 - 2 * lambda / n_tuples * stepsize;
    if (wscale > 0) { incrModel *= wscale; }
    else { incrModel.setZero(); }
}

template <class Model, class Hessian>
void
L2<Model, Hessian>::hessian(
        const model_type                    &model,
        const double                        &lambda,
        hessian_type                        &hessian) {
    int n = model.size();
    hessian += 2 * lambda * hessian.Identity(n, n);
}

template <class Model, class Hessian>
double
L2<Model, Hessian>::loss(
        const model_type                    &model,
        const double                        &lambda) {
    return lambda * model.norm();
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif


