/* ----------------------------------------------------------------------- *//**
 *
 * @file linear_svm.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TASK_LINEAR_SVM_HPP_
#define MADLIB_MODULES_CONVEX_TASK_LINEAR_SVM_HPP_

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
    typedef typename Tuple::independent_variables_type independent_variables_type;
    typedef typename Tuple::dependent_variable_type dependent_variable_type;
    typedef typename model_type::PlainEigenType model_eigen_type;

    static double epsilon;
    static bool is_svc;

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

    static double lossAndGradient(
            model_type                          &model,
            const Matrix                        &x,
            const ColumnVector                  &y,
            model_eigen_type                    &gradient);
    static double loss(
            const model_type                    &model,
            const independent_variables_type    &x,
            const dependent_variable_type       &y);

    static dependent_variable_type predict(
            const model_type                    &model,
            const independent_variables_type    &x);
};

template <class Model, class Tuple>
double LinearSVM<Model, Tuple >::epsilon = 0.;

template <class Model, class Tuple>
bool LinearSVM<Model, Tuple >::is_svc = false;

template <class Model, class Tuple>
void
LinearSVM<Model, Tuple>::gradient(
        const model_type                    &model,
        const independent_variables_type    &x,
        const dependent_variable_type       &y,
        model_type                          &gradient) {
    double wx = dot(model, x);
    if (is_svc) {
        if (1 - wx * y > 0) {
            double c = -y;   // minus for "-loglik"
            gradient += c * x;
        }
    }
    else {
        double wx_y = wx - y;
        double c = wx_y > 0 ? 1. : -1.;
        if (c*wx_y - epsilon > 0.)
            gradient += c * x;
    }
}

template <class Model, class Tuple>
void
LinearSVM<Model, Tuple>::gradientInPlace(
        model_type                          &model,
        const independent_variables_type    &x,
        const dependent_variable_type       &y,
        const double                        &stepsize) {
    double wx = dot(model, x);
    if (is_svc) {
        if (1. - wx * y > 0.) {
            double c = -y;   // minus for "-loglik"
            model -= stepsize * c * x;
        }
    }
    else {
        double wx_y = wx - y;
        double c = wx_y > 0 ? 1. : -1.;
        if (c*wx_y - epsilon > 0.)
            model -= stepsize * c * x;
    }
}

template <class Model, class Tuple>
double
LinearSVM<Model, Tuple>::lossAndGradient(
        model_type                          &model,
        const Matrix                        &x,
        const ColumnVector                  &y,
        model_eigen_type                    &gradient) {

    gradient.setZero();
    model_eigen_type s = x * model;
    double l = 0.0;
    int n = s.rows();
    double dist = 0.;
    double c = 0.;

    for (int i=0; i<n; i++) {
        if (is_svc) {
            c = -y(i);   // minus for "-loglik"
            dist = 1. - s(i) * y(i);
        } else {
            double wx_y = s(i) - y(i);
            c = wx_y > 0 ? 1. : -1.;
            dist = c * wx_y - epsilon;
        }
        if ( dist > 0.) {
            gradient += c * x.row(i);
            l += dist;
        }

    }
    l /= n;
    gradient.array() /= n;
    return l;
}

template <class Model, class Tuple>
double
LinearSVM<Model, Tuple>::loss(
        const model_type                    &model,
        const independent_variables_type    &x,
        const dependent_variable_type       &y) {
    double wx = dot(model, x);
    double distance = 0.;
    if (is_svc) {
        distance = 1. - wx * y;
    }
    else {
        distance = fabs(wx - y) - epsilon;
    }
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
