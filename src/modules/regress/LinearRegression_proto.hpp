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

} // namespace regress

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_PROTO_HPP)
