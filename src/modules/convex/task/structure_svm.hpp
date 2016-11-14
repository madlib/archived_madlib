/* ----------------------------------------------------------------------- *//**
 *
 * @file structure_svm.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TASK_STRUCTURE_SVM_HPP_
#define MADLIB_MODULES_CONVEX_TASK_STRUCTURE_SVM_HPP_

#include <dbconnector/dbconnector.hpp>

namespace madlib {

namespace modules {

namespace convex {

// Use Eigen
using namespace madlib::dbal::eigen_integration;


template <class Model, class Tuple>
class StructureSVM {
public:
    typedef Model model_type;
    typedef Tuple tuple_type;
    typedef typename Tuple::independent_variables_type
        independent_variables_type;
    typedef typename Tuple::dependent_variable_type dependent_variable_type;

    static int batchSize;
    static int nEpochs;

    static double gradientInPlace(
            model_type                          &model,
            const independent_variables_type    &x,
            const dependent_variable_type       &y,
            const double                        &stepsize,
            const double                        &reg);

private:
    static double __lossAndGradient(
            const model_type                    &model,
            const Matrix                        &x,
            const ColumnVector                  &y,
            Matrix                              &gradient);
};

template <class Model, class Tuple>
int StructureSVM<Model, Tuple >::batchSize = 64;

template <class Model, class Tuple>
int StructureSVM<Model, Tuple >::nEpochs = 1;

template <class Model, class Tuple>
double
StructureSVM<Model, Tuple>::__lossAndGradient(
        const model_type                    &model,
        const Matrix                        &x,
        const ColumnVector                  &y,
        Matrix                              &gradient) {

    gradient.setZero();
    Matrix s = x*model;
    double l = 0.0;
    int n = s.rows(), m = s.cols();
    for (int i=0; i<n; i++) {
        Matrix si = s.row(i);
        double ct = si(y(i));
        for (int c=0; c<m; c++) {
            if (c==y(i)) continue;
            double g = si(c) - ct + 1;
            if (g>0) {
                l += g;
                gradient.col(c) += x.row(i);
                gradient.col(y(i)) -= x.row(i);
            }
        }
    }
    l /= n;
    gradient.array() /= n;
    return l;
}

template <class Model, class Tuple>
double
StructureSVM<Model, Tuple>::gradientInPlace(
        model_type                          &model,
        const independent_variables_type    &x,
        const dependent_variable_type       &y,
        const double                        &stepsize,
        const double                        &reg) {

    int N = x.rows(), d = x.cols();
    int iter_per_epochs = N < batchSize ? 1 : N / batchSize;
    Matrix gradient(model);
    double total_loss = 0.0;
    int counter = 0;
    for (int i=0; i<nEpochs; i++) {
        double loss = 0.0;
        for (int j=0, k=0; j<iter_per_epochs; j++, k+=batchSize) {
            Matrix X_batch;
            ColumnVector y_batch;
            if (j==iter_per_epochs-1) {
                X_batch = x.bottomRows(N-k);
                y_batch = y.tail(N-k);
            } else {
                X_batch = x.block(k, 0, batchSize, d);
                y_batch = y.segment(k, batchSize);
            }
            loss += __lossAndGradient(model, X_batch, y_batch, gradient);
            model -= stepsize*(gradient + reg*model);
            total_loss += loss;
            counter++;
        }
        loss /= iter_per_epochs;
    }

    total_loss /= counter;

    return total_loss;

}

} // namespace convex

} // namespace modules

} // namespace madlib

#endif
