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

// -----------------------------------------------------------------------
// Linear regression
// -----------------------------------------------------------------------
typedef LinearRegressionAccumulator<RootContainer> LinRegrState;
typedef LinearRegressionAccumulator<MutableRootContainer> MutableLinRegrState;

AnyType
linregr_transition::run(AnyType& args) {
    MutableLinRegrState state = args[0].getAs<MutableByteString>();
    if (args[1].isNull() || args[2].isNull()) { return args[0]; }
    double y = args[1].getAs<double>();
    MappedColumnVector x;
    try {
        MappedColumnVector xx = args[2].getAs<MappedColumnVector>();
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

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
    tuple << result.coef
          << result.r2
          << result.stdErr
          << result.tStats
          << (state.numRows > state.widthOfX ? result.pValues : Null())
          << sqrt(result.conditionNo)
          << static_cast<uint64_t>(state.numRows)
          << result.vcov;
    return tuple;
}
// -----------------------------------------------------------------------


// -----------------------------------------------------------------------
// Robust linear regression variance estimate using the Huber-White estimator
// -----------------------------------------------------------------------
typedef RobustLinearRegressionAccumulator<RootContainer> RobustLinRegrState;
typedef RobustLinearRegressionAccumulator<MutableRootContainer> MutableRobustLinRegrState;

AnyType
robust_linregr_transition::run(AnyType& args) {
    MutableRobustLinRegrState state = args[0].getAs<MutableByteString>();
    if (args[1].isNull() || args[2].isNull()) { return args[0]; }
    double y = args[1].getAs<double>();
    MappedColumnVector x;
    try {
        MappedColumnVector xx = args[2].getAs<MappedColumnVector>();
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }
    MappedColumnVector coef = args[3].getAs<MappedColumnVector>();

    state << RobustLinRegrState::tuple_type(x, y, coef);
    return state.storage();
}

AnyType
robust_linregr_merge_states::run(AnyType& args) {
    MutableRobustLinRegrState stateLeft = args[0].getAs<MutableByteString>();
    RobustLinRegrState stateRight = args[1].getAs<ByteString>();

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
robust_linregr_final::run(AnyType& args) {
    RobustLinRegrState state = args[0].getAs<ByteString>();

    // If we haven't seen any data, just return Null. This is the standard
    // behavior of aggregate function on empty data sets (compare, e.g.,
    // how PostgreSQL handles sum or avg on empty inputs)
    if (state.numRows == 0)
        return Null();

    AnyType tuple;
    RobustLinearRegression result(state);

    tuple << result.coef
          << result.stdErr
          << result.tStats
          << (state.numRows > state.widthOfX
              ? result.pValues
              : Null());
    return tuple;
}
// -----------------------------------------------------------------------


// -----------------------------------------------------------------------
// Breusch–Pagan test for heteroskedasticity.
// This is the first step of the test and does not include correction for the
// standard errors if the data is heteroskedastic.
// -----------------------------------------------------------------------

typedef HeteroLinearRegressionAccumulator<RootContainer> HeteroLinRegrState;
typedef HeteroLinearRegressionAccumulator<MutableRootContainer>
                                                    MutableHeteroLinRegrState;

AnyType
hetero_linregr_transition::run(AnyType& args) {
    MutableHeteroLinRegrState state = args[0].getAs<MutableByteString>();
    if (args[1].isNull() || args[2].isNull()) { return args[0]; }
    double y = args[1].getAs<double>();
    MappedColumnVector x;
    try {
        MappedColumnVector xx = args[2].getAs<MappedColumnVector>();
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }
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
// -----------------------------------------------------------------------

} // namespace regress

} // namespace modules

} // namespace madlib
