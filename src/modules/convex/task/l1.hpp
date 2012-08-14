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

    static void gradient(
            const model_type                    &model,
            const double                        &lambda, 
            model_type                          &gradient);

    static double loss(
            const model_type                    &model, 
            const double                        &lambda);

private:
    static double sign(const double & x) {
        if (x == 0.) { return 0.; }
        else { return x > 0. ? 1. : -1.; }
    }
};

template <class Model>
void
L1<Model>::gradient(
        const model_type                    &model,
        const double                        &lambda, 
        model_type                          &gradient) {
    Index i;
    for (i = 0; i < model.rows(); i ++) {
        if (model(i) == 0.) {
            if (std::abs(gradient(i)) > lambda) {
                gradient(i) -= lambda * sign(gradient(i));
            } else { gradient(i) = 0.; }
        } else { gradient(i) += lambda * sign(model(i)); }
    }
}

template <class Model>
double 
L1<Model>::loss(
        const model_type                    &model, 
        const double                        &lambda) {
    double norm = 0.;
    Index i;
    for (i = 0; i < model.rows(); i ++) {
        norm += std::abs(model(i));
    }
    return lambda * norm;
}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

