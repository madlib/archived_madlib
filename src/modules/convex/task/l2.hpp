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
        model_type                          &gradient) 
{
    Index i;
    for (i = 0; i < model.rows()-1; i++)
        gradient(i) += lambda * model(i);
}

template <class Model, class Hessian>
void
L2<Model, Hessian>::hessian(
        const model_type                    &model,
        const double                        &lambda, 
        hessian_type                        &hessian) 
{
    int n = model.rows();
    hessian += lambda * hessian.Identity(n, n);
    hessian(n-1, n-1) -= lambda;
}

template <class Model, class Hessian>
double 
L2<Model, Hessian>::loss(
        const model_type                    &model, 
        const double                        &lambda) 
{
    Index i;
    double s = 0.;
    for (i = 0; i < model.rows()-1; i++)
        s += model(i) * model(i);
    return lambda * s / 2.;
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif


