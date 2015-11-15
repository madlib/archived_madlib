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

    static double lambda;
    static int n_tuples;

    static void gradient(
            const model_type &model,
            model_type &gradient);

    static void scaling(
            model_type &model,
            const double &stepsize);

    static void hessian(
            const model_type &model,
            hessian_type &hessian);

    static double loss(
            const model_type &model);
};

template <class Model, class Hessian>
double
L2<Model, Hessian>::lambda = 0.;

template <class Model, class Hessian>
int
L2<Model, Hessian>::n_tuples = 1;

template <class Model, class Hessian>
void
L2<Model, Hessian>::gradient(
        const model_type &model,
        model_type &gradient) {
    // 1/2 * lambda * || w ||^2
    gradient += lambda * model;
}

template <class Model, class Hessian>
void
L2<Model, Hessian>::scaling(
        model_type &model,
        const double &stepsize) {
    double wscale = 1 - lambda / n_tuples * stepsize;
    if (wscale > 0) { model *= wscale; }
    else { model.setZero(); }
}

template <class Model, class Hessian>
void
L2<Model, Hessian>::hessian(
        const model_type &model,
        hessian_type &hessian) {
    int n = model.size();
    hessian += lambda * hessian.Identity(n, n);
}

template <class Model, class Hessian>
double
L2<Model, Hessian>::loss(
        const model_type &model) {
    // 1/2 * lambda * || w ||^2
    return lambda * model.norm()*model.norm() / 2;
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif


