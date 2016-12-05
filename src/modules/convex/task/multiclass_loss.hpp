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
class SoftmaxCrossEntropy {
public:
    typedef Model model_type;
    typedef Tuple tuple_type;
    typedef typename Tuple::independent_variables_type
        independent_variables_type;
    typedef typename Tuple::dependent_variable_type dependent_variable_type;
    typedef typename model_type::PlainEigenType model_eigen_type;

    static inline double lossAndGradient(
            const model_type                    &model,
            const Matrix                        &x,
            const ColumnVector                  &y,
            model_eigen_type                    &gradient) {
        gradient.setZero();
        model_eigen_type probs = x*model;
        probs.colwise() -= probs.rowwise().maxCoeff();
        probs.array() = probs.array().exp();
        probs = probs.array().colwise() / probs.rowwise().sum().array();
        double l = 0.0;
        int n = probs.rows();
        for (int i=0; i<n; i++) {
            l -= log(probs(i, y(i)));
            gradient += x.row(i).transpose() * probs.row(i);
            gradient.col(y(i)) -= x.row(i);
        }
        l /= n;
        gradient.array() /= n;
        return l;
    }
};


template <class Model, class Tuple>
class StructureHinge {
public:
    typedef Model model_type;
    typedef Tuple tuple_type;
    typedef typename Tuple::independent_variables_type
        independent_variables_type;
    typedef typename Tuple::dependent_variable_type dependent_variable_type;
    typedef typename model_type::PlainEigenType model_eigen_type;

    static inline double lossAndGradient(
            const model_type                    &model,
            const Matrix                        &x,
            const ColumnVector                  &y,
            model_eigen_type                    &gradient) {
        gradient.setZero();
        model_eigen_type s = x*model;
        double l = 0.0;
        int n = s.rows(), m = s.cols();
        for (int i=0; i<n; i++) {
            model_eigen_type si = s.row(i);
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
};


template <class Model, class Tuple>
class StructureSquaredHinge {
public:
    typedef Model model_type;
    typedef Tuple tuple_type;
    typedef typename Tuple::independent_variables_type
        independent_variables_type;
    typedef typename Tuple::dependent_variable_type dependent_variable_type;
    typedef typename model_type::PlainEigenType model_eigen_type;

    static inline double lossAndGradient(
            const model_type                    &model,
            const Matrix                        &x,
            const ColumnVector                  &y,
            model_eigen_type                    &gradient) {
        gradient.setZero();
        model_eigen_type s = x*model;
        double l = 0.0;
        int n = s.rows(), m = s.cols();
        for (int i=0; i<n; i++) {
            model_eigen_type si = s.row(i);
            double ct = si(y(i));
            for (int c=0; c<m; c++) {
                if (c==y(i)) continue;
                double g = si(c) - ct + 1;
                if (g>0) {
                    l += g*g;
                    gradient.col(c) += 2*g*x.row(i);
                    gradient.col(y(i)) -= 2*g*x.row(i);
                }
            }
        }
        l /= n;
        gradient.array() /= n;
        return l;
    }
};


} // namespace convex

} // namespace modules

} // namespace madlib

#endif
