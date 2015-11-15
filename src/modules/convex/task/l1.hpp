/* ----------------------------------------------------------------------- *//**
 *
 * @file l1.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TASK_L1_HPP_
#define MADLIB_MODULES_CONVEX_TASK_L1_HPP_

#include <dbconnector/dbconnector.hpp>

namespace madlib {

namespace modules {

namespace convex {

// Use Eigen
using namespace madlib::dbal::eigen_integration;

template <class Model>
class L1 {
public:
    typedef Model model_type;

    static double lambda;

    static int n_tuples;

    static void gradient(
            const model_type &model,
            model_type &gradient);

    static void clipping(
            model_type &model,
            const double &stepsize);

    static double loss(
            const model_type &model);
};

template <class Model>
double
L1<Model>::lambda = 0.;

template <class Model>
int
L1<Model>::n_tuples = 1;

template <class Model>
void
L1<Model>::gradient(
        const model_type &model,
        model_type &gradient) {
    if (lambda != 0.) {
        for (Index i = 0; i < model.size(); i++) {
            if (model(i) > 0) {
                gradient(i) += lambda;
            } else if (model(i) < 0) {
                gradient(i) -= lambda;
            }
        }
    }
}

template <class Model>
void
L1<Model>::clipping(
        model_type &model,
        const double &stepsize) {
    if (lambda != 0.) {
        // implement the Clipping method mentioned in Tsuruoka et al. 2009
        double clip_boundry = lambda / n_tuples * stepsize;
        for (Index i = 0; i < model.size(); i++) {
            if (model(i) > clip_boundry) {
                model(i) -= clip_boundry;
            } else if (model(i) < - clip_boundry) {
                model(i) += clip_boundry;
            } else { model(i) = 0.; }
        }
    }
}

template <class Model>
double
L1<Model>::loss(
    const model_type &model) {
    double s = 0.;
    if (lambda != 0.)
        for (Index i = 0; i < model.size(); i++) {
            s += std::abs(model(i));
        }
    return lambda * s;
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif


