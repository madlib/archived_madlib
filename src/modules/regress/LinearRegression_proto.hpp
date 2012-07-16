/* ----------------------------------------------------------------------- *//**
 *
 * @file LinearRegression_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_PROTO_HPP
#define MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_PROTO_HPP

namespace madlib {

namespace modules {

namespace regress {

// Use Eigen
using namespace dbal::eigen_integration;

template <class Parent>
class LinearRegressionAccumulator
  : public State<LinearRegressionAccumulator<Parent>, Parent> {
public:
    MADLIB_STATE_TYPEDEFS(LinearRegressionAccumulator, Parent)

    LinearRegressionAccumulator(Init_type& inInitialization);
    void bind(BinaryStream_type& inStream);
    LinearRegressionAccumulator& feed(MappedColumnVector x, double y);
    template <class OtherParent> LinearRegressionAccumulator& feed(
        const LinearRegressionAccumulator<OtherParent>& inOther);
    template <class OtherParent> LinearRegressionAccumulator& operator=(
        const LinearRegressionAccumulator<OtherParent>& inOther);

    Ref<uint64_type> numRows;
    Ref<uint16_type> widthOfX;
    Ref<double_type> y_sum;
    Ref<double_type> y_square_sum;
    MappedColumnVector_type X_transp_Y;
    MappedMatrix_type X_transp_X;
};

class LinearRegression {
public:
    template <class Parent> LinearRegression(
        const LinearRegressionAccumulator<Parent>& inState);
    template <class Parent> LinearRegression& compute(
        const LinearRegressionAccumulator<Parent>& inState);

    MutableMappedColumnVector coef;
    double r2;
    MutableMappedColumnVector stdErr;
    MutableMappedColumnVector tStats;
    MutableMappedColumnVector pValues;
    double conditionNo;
};

} // namespace regress

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_PROTO_HPP)
