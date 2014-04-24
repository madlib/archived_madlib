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
    typedef DynamicStruct<LinearRegressionAccumulator, Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;
    typedef std::tuple<MappedColumnVector, double> tuple_type;

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
    ColumnVector_type X_transp_Y;
    Matrix_type X_transp_X;
};

class LinearRegression {
public:
    template <class Container> LinearRegression(
        const LinearRegressionAccumulator<Container>& inState);
    template <class Container> LinearRegression& compute(
        const LinearRegressionAccumulator<Container>& inState);

    MutableNativeColumnVector coef;
    double r2;
    MutableNativeColumnVector stdErr;
    MutableNativeColumnVector tStats;
    MutableNativeColumnVector pValues;
    double conditionNo;
    MutableNativeMatrix vcov;
};

// ------------------------------------------------------------------------

template <class Container>
class RobustLinearRegressionAccumulator
  : public DynamicStruct<RobustLinearRegressionAccumulator<Container>, Container> {
public:
    typedef DynamicStruct<RobustLinearRegressionAccumulator, Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;
    typedef std::tuple<MappedColumnVector, double, MappedColumnVector> tuple_type;

    RobustLinearRegressionAccumulator(Init_type& inInitialization);
    void bind(ByteStream_type& inStream);
    RobustLinearRegressionAccumulator& operator<<(const tuple_type& inTuple);
    template <class OtherContainer> RobustLinearRegressionAccumulator& operator<<(
        const RobustLinearRegressionAccumulator<OtherContainer>& inOther);
    template <class OtherContainer> RobustLinearRegressionAccumulator& operator=(
        const RobustLinearRegressionAccumulator<OtherContainer>& inOther);

    uint64_type numRows;
    uint16_type widthOfX;
    ColumnVector_type ols_coef;
    Matrix_type X_transp_X;
    Matrix_type X_transp_r2_X;
};

class RobustLinearRegression {
public:
    template <class Container> RobustLinearRegression(
        const RobustLinearRegressionAccumulator<Container>& inState);
    template <class Container> RobustLinearRegression& compute(
        const RobustLinearRegressionAccumulator<Container>& inState);

    MutableNativeColumnVector coef;
    MutableNativeColumnVector stdErr;
    MutableNativeColumnVector tStats;
    MutableNativeColumnVector pValues;
};

// ------------------------------------------------------------------------
// Accumulator class for testing heteroskedasticity
template <class Container>
class HeteroLinearRegressionAccumulator
  : public DynamicStruct<HeteroLinearRegressionAccumulator<Container>, Container>
{
  public:
    typedef DynamicStruct<HeteroLinearRegressionAccumulator, Container> Base;
    MADLIB_DYNAMIC_STRUCT_TYPEDEFS;

    typedef std::tuple<MappedColumnVector, double, MappedColumnVector> hetero_tuple_type;

    HeteroLinearRegressionAccumulator(Init_type& inInitialization);
    void bind(ByteStream_type& inStream);

    HeteroLinearRegressionAccumulator& operator<<(const hetero_tuple_type& inTuple);
    template <class OtherContainer> HeteroLinearRegressionAccumulator& operator<<(
        const HeteroLinearRegressionAccumulator<OtherContainer>& inOther);
    template <class OtherContainer> HeteroLinearRegressionAccumulator& operator=(
        const HeteroLinearRegressionAccumulator<OtherContainer>& inOther);

    uint64_type numRows;
    uint16_type widthOfX;
    double_type a_sum;
    double_type a_square_sum;
    ColumnVector_type X_transp_A;
    Matrix_type X_transp_X;
};

class HeteroLinearRegression
{
  public:
    template <class Container> HeteroLinearRegression(
        const HeteroLinearRegressionAccumulator<Container>& inState);
    template <class Container> HeteroLinearRegression& compute(
        const HeteroLinearRegressionAccumulator<Container>& inState);

    double test_statistic;
    double pValue;
};

} // namespace regress

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_PROTO_HPP)
