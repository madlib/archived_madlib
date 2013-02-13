/* ----------------------------------------------------------------------- *//**
 *
 * @file linear.cpp
 *
 * @brief Linear-regression functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#include "LinearRegression_proto.hpp"
#include "LinearRegression_impl.hpp"
#include "linear.hpp"

namespace madlib {

namespace modules {

namespace regress {

typedef LinearRegressionAccumulator<RootContainer> LinRegrState;
typedef LinearRegressionAccumulator<MutableRootContainer> MutableLinRegrState;

// Heteroskedasticitic regression
typedef HeteroLinearRegressionAccumulator<RootContainer> HeteroLinRegrState;
typedef HeteroLinearRegressionAccumulator<MutableRootContainer>	MutableHeteroLinRegrState;

AnyType
linregr_transition::run(AnyType& args) {
    MutableLinRegrState state = args[0].getAs<MutableByteString>();
    double y = args[1].getAs<double>();
    MappedColumnVector x = args[2].getAs<MappedColumnVector>();

    state << MutableLinRegrState::tuple_type(x, y);
    return state.storage();
}

AnyType
linregr_merge_states::run(AnyType& args) {
    MutableLinRegrState stateLeft = args[0].getAs<MutableByteString>();
    LinRegrState stateRight = args[1].getAs<ByteString>();

    stateLeft << stateRight;
    return stateLeft.storage();
}

AnyType
linregr_final::run(AnyType& args) {
    LinRegrState state = args[0].getAs<ByteString>();

    // If we haven't seen any data, just return Null. This is the standard
    // behavior of aggregate function on empty data sets (compare, e.g.,
    // how PostgreSQL handles sum or avg on empty inputs)
    if (state.numRows == 0)
        return Null();

    AnyType tuple;
    LinearRegression result(state);
    tuple << result.coef << result.r2 << result.stdErr << result.tStats
          << (state.numRows > state.widthOfX
              ? result.pValues
              : Null())
          << sqrt(result.conditionNo);
    return tuple;
}

/*
  Breusch–Pagan test for heteroskedasticity.
  This is the first step of the test and does not include correction for the
  standard errors if the data is heteroskedastic.
*/

AnyType
hetero_linregr_transition::run(AnyType& args) {
    MutableHeteroLinRegrState state = args[0].getAs<MutableByteString>();
    double y = args[1].getAs<double>();
    MappedColumnVector x = args[2].getAs<MappedColumnVector>();
    MappedColumnVector coef = args[3].getAs<MappedColumnVector>();

   state << MutableHeteroLinRegrState::hetero_tuple_type(x, y, coef);
    return state.storage();
}

AnyType
hetero_linregr_merge_states::run(AnyType& args) {
    MutableHeteroLinRegrState stateLeft = args[0].getAs<MutableByteString>();
    HeteroLinRegrState stateRight = args[1].getAs<ByteString>();

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.numRows == 0) {
        return stateRight.storage();
    } else if (stateRight.numRows == 0) {
        return stateLeft.storage();
    }

    stateLeft << stateRight;
    return stateLeft.storage();
}

AnyType
hetero_linregr_final::run(AnyType& args) {
    HeteroLinRegrState state = args[0].getAs<ByteString>();

    // If we haven't seen any data, just return Null. This is the standard
    // behavior of aggregate function on empty data sets (compare, e.g.,
    // how PostgreSQL handles sum or avg on empty inputs)
    if (state.numRows == 0)
        return Null();

    AnyType tuple;
    HeteroLinearRegression result(state);

    tuple << result.test_statistic << result.pValue;

    return tuple;
}

} // namespace regress

} // namespace modules

} // namespace madlib
