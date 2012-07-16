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

typedef LinearRegressionAccumulator<RootState<ByteString> >
    LinRegrState;
typedef LinearRegressionAccumulator<RootState<MutableByteString> >
    MutableLinRegrState;

AnyType
linregr_transition::run(AnyType &args) {
    MutableLinRegrState state = args[0].getAs<MutableByteString>();
    double y = args[1].getAs<double>();
    MappedColumnVector x = args[2].getAs<MappedColumnVector>();

    return state.feed(x, y).storage();
}

AnyType
linregr_merge_states::run(AnyType &args) {
    MutableLinRegrState stateLeft = args[0].getAs<MutableByteString>();
    LinRegrState stateRight = args[1].getAs<ByteString>();

    return stateLeft.feed(stateRight).storage();
}

AnyType
linregr_final::run(AnyType &args) {
    LinRegrState state = args[0].getAs<ByteString>();
    LinearRegression result(state);

    // If we haven't seen any data, just return Null. This is the standard
    // behavior of aggregate function on empty data sets (compare, e.g.,
    // how PostgreSQL handles sum or avg on empty inputs)
    if (state.numRows == 0)
        return Null();

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << result.coef << result.r2 << result.stdErr << result.tStats
        << (state.numRows > state.widthOfX
            ? result.pValues
            : Null())
        << result.conditionNo;
    return tuple;
}

} // namespace regress

} // namespace modules

} // namespace madlib
