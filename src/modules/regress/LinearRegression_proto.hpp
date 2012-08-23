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
using namespace dbal;
using namespace dbal::eigen_integration;

template <class Container>
class LinearRegressionAccumulator
  : public DynamicStruct<LinearRegressionAccumulator<Container>, Container> {
public:
    enum { isMutable = Container::isMutable };
    typedef std::tuple<MappedColumnVector, double> tuple_type;

    MADLIB_DYNAMIC_STRUCT_TYPEDEFS(LinearRegressionAccumulator, Container)

    LinearRegressionAccumulator(Init_type& inInitialization);
    void bind(ByteStream_type& inStream);

    LinearRegressionAccumulator& operator<<(const tuple_type& inTuple);
    template <class OtherContainer> LinearRegressionAccumulator& operator<<(
        const LinearRegressionAccumulator<OtherContainer>& inOther);
    template <class OtherContainer> LinearRegressionAccumulator& operator=(
        const LinearRegressionAccumulator<OtherContainer>& inOther);

    uint64_type numRows;
    uint16_type widthOfX;
    double_type y_sum;
    double_type y_square_sum;
    MappedColumnVector_type X_transp_Y;
    MappedMatrix_type X_transp_X;
};

class LinearRegression {
public:
    template <class Container> LinearRegression(
        const LinearRegressionAccumulator<Container>& inState);
    template <class Container> LinearRegression& compute(
        const LinearRegressionAccumulator<Container>& inState);

    MutableMappedColumnVector coef;
    double r2;
    MutableMappedColumnVector stdErr;
    MutableMappedColumnVector tStats;
    MutableMappedColumnVector pValues;
    double conditionNo;
};

// Accumulator class for Huber-White estimator
template <class Container>
class RobustLinearRegressionAccumulator
  : public DynamicStruct<RobustLinearRegressionAccumulator<Container>, Container> {
public:
    enum { isMutable = Container::isMutable };
    typedef std::tuple<MappedColumnVector, double, MappedColumnVector> tuple_type;

    MADLIB_DYNAMIC_STRUCT_TYPEDEFS(RobustLinearRegressionAccumulator, Container)

    RobustLinearRegressionAccumulator(Init_type& inInitialization);
    void bind(ByteStream_type& inStream);

    RobustLinearRegressionAccumulator& operator<<(const tuple_type& inTuple);
    template <class OtherContainer> RobustLinearRegressionAccumulator& operator<<(
        const RobustLinearRegressionAccumulator<OtherContainer>& inOther);
    template <class OtherContainer> RobustLinearRegressionAccumulator& operator=(
        const RobustLinearRegressionAccumulator<OtherContainer>& inOther);

    uint64_type numRows;
    uint16_type widthOfX;
    MappedColumnVector_type ols_coef;
    MappedMatrix_type X_transp_X;
    MappedMatrix_type X_transp_r2_X;

};

class RobustLinearRegression {
public:
    template <class Container> RobustLinearRegression(
        const RobustLinearRegressionAccumulator<Container>& inState);
    template <class Container> RobustLinearRegression& compute(
        const RobustLinearRegressionAccumulator<Container>& inState);
				
    MutableMappedColumnVector stdErr;
    MutableMappedColumnVector tStats;
    MutableMappedColumnVector pValues;
    double conditionNo;

};

} // namespace regress

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_PROTO_HPP)
