/* ----------------------------------------------------------------------- *//**
 * 
 * @file ols.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TASK_OLS_HPP_
#define MADLIB_MODULES_CONVEX_TASK_OLS_HPP_

#include <dbconnector/dbconnector.hpp>
#include <fstream>
namespace madlib {

namespace modules {

namespace convex {

// Use Eigen
using namespace madlib::dbal::eigen_integration;

template <class Model, class Tuple, class Hessian = Matrix>
class OLS {
public:
    typedef Model model_type;
    typedef Tuple tuple_type;
    typedef Hessian hessian_type;
    typedef typename Tuple::independent_variables_type 
        independent_variables_type;
    typedef typename Tuple::dependent_variable_type dependent_variable_type;

    static void gradient(
            const model_type                    &model,
            const independent_variables_type    &x,
            const dependent_variable_type       &y, 
            model_type                          &gradient);

    static void hessian(
            const model_type                    & /* model */,
            const independent_variables_type    &x,
            const dependent_variable_type       & /* y */, 
            hessian_type                        &hessian);

    static double loss(
            const model_type                    &model, 
            const independent_variables_type    &x, 
            const dependent_variable_type       &y);
    
    static dependent_variable_type predict(
            const model_type                    &model,
            const double  &intercept,
            const independent_variables_type    &x);
};

template <class Model, class Tuple, class Hessian>
void
OLS<Model, Tuple, Hessian>::gradient(
        const model_type                    &model,
        const independent_variables_type    &x,
        const dependent_variable_type       &y,
        model_type                          &gradient) 
{
    double wx = dot(model, x);
    double r = wx - y;
    gradient += r * x;
}

template <class Model, class Tuple, class Hessian>
void
OLS<Model, Tuple, Hessian>::hessian(
        const model_type                    & /* model */,
        const independent_variables_type    &x,
        const dependent_variable_type       & /* y */, 
        hessian_type                        &hessian) 
{
    hessian += x * trans(x);
}

template <class Model, class Tuple, class Hessian>
double 
OLS<Model, Tuple, Hessian>::loss(
        const model_type                    &model, 
        const independent_variables_type    &x, 
        const dependent_variable_type       &y) 
{
    double wx = dot(model, x);
    return (wx - y) * (wx - y) / 2.;
}

template <class Model, class Tuple, class Hessian>
typename Tuple::dependent_variable_type
OLS<Model, Tuple, Hessian>::predict(
        const model_type                    &model,
        const double    &intercept,
        const independent_variables_type    &x) {
    // double wx = dot(model, x)
    double wx = dot(model, x) + intercept;
    return wx;
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

