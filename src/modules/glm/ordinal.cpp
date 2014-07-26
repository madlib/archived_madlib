/* ----------------------------------------------------------------------- *//**
 *
 * @file ordinal.cpp
 *
 * @brief ordinal linear model functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/prob/student.hpp>

#include "Ordinal_proto.hpp"
#include "Ordinal_impl.hpp"
#include "ordinal.hpp"

namespace madlib {

namespace modules {

namespace glm {

// types of states
typedef OrdinalAccumulator<RootContainer> OrdinalState;
typedef OrdinalAccumulator<MutableRootContainer> OrdinalMutableState;

// Logit link
// ------------------------------------------------------------------------
typedef OrdinalAccumulator<MutableRootContainer,Multinomial,OrdinalLogit>
    MutableOrdinalLogitState;

AnyType
ordinal_logit_transition::run(AnyType& args) {
    MutableOrdinalLogitState state = args[0].getAs<MutableByteString>();
    if (state.terminated || args[1].isNull() || args[2].isNull()) {
        return args[0];
    }
    double y = args[1].getAs<double>();
    MappedColumnVector x;
    try {
        MappedColumnVector xx = args[2].getAs<MappedColumnVector>();
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }
    if (state.empty()) {
        state.num_features = static_cast<uint16_t>(x.size());
        state.num_categories = args[4].getAs<uint16_t>();
        state.optimizer.num_coef = static_cast<uint16_t>(state.num_features + (state.num_categories-1));
        state.resize();
        if (!args[3].isNull()) {
            OrdinalState prev_state = args[3].getAs<ByteString>();
            state = prev_state;
            state.reset();
        }
    }
    state << OrdinalMutableState::tuple_type(x, y);
    return state.storage();
}

// ------------------------------------------------------------------------

// Probit link
// ------------------------------------------------------------------------
typedef OrdinalAccumulator<MutableRootContainer,Multinomial,OrdinalProbit>
    MutableOrdinalProbitState;

AnyType
ordinal_probit_transition::run(AnyType& args) {
    MutableOrdinalProbitState state = args[0].getAs<MutableByteString>();
    if (state.terminated || args[1].isNull() || args[2].isNull()) {
        return args[0];
    }
    double y = args[1].getAs<double>();
    MappedColumnVector x;
    try {
        MappedColumnVector xx = args[2].getAs<MappedColumnVector>();
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }
    if (state.empty()) {
        state.num_features = static_cast<uint16_t>(x.size());
        state.num_categories = args[4].getAs<uint16_t>();
        state.optimizer.num_coef = static_cast<uint16_t>(
                state.num_features + (state.num_categories-1));
        state.resize();
        if (!args[3].isNull()) {
            OrdinalState prev_state = args[3].getAs<ByteString>();
            state = prev_state;
            state.reset();
        }
    }
    state << OrdinalMutableState::tuple_type(x, y);
    return state.storage();
}

// ------------------------------------------------------------------------

AnyType
ordinal_merge_states::run(AnyType& args) {
    OrdinalMutableState stateLeft = args[0].getAs<MutableByteString>();
    OrdinalState stateRight = args[1].getAs<ByteString>();

    stateLeft << stateRight;
    return stateLeft.storage();
}

AnyType
ordinal_final::run(AnyType& args) {
    OrdinalMutableState state = args[0].getAs<MutableByteString>();

    // If we haven't seen any valid data, just return Null. This is the standard
    // behavior of aggregate function on empty data sets (compare, e.g.,
    // how PostgreSQL handles sum or avg on empty inputs)
    if (state.empty() || state.terminated) { return Null(); }
    state.apply();
    if (state.empty() || state.terminated) { return Null(); }
    return state.storage();
}

// ------------------------------------------------------------------------

AnyType
ordinal_result::run(AnyType& args) {
    if (args[0].isNull()) { return Null(); }

    OrdinalState state = args[0].getAs<ByteString>();
    OrdinalResult result(state);

    AnyType tuple;
    tuple << result.coef_alpha
          << result.std_err_alpha
          << result.z_stats_alpha
          << result.p_values_alpha
          << result.loglik
          << result.coef_beta
          << result.std_err_beta
          << result.z_stats_beta
          << result.p_values_beta
          << result.num_rows_processed;

    return tuple;
}

// ------------------------------------------------------------------------

AnyType
ordinal_loglik_diff::run(AnyType& args) {
    if (args[0].isNull() || args[1].isNull()) {
        return std::numeric_limits<double>::infinity();
    } else {
        OrdinalState state0 = args[0].getAs<ByteString>();
        OrdinalState state1 = args[1].getAs<ByteString>();
        double a = state0.loglik;
        double b = state1.loglik;
        if (a >= 0. || b >= 0.) { return 0.; } // probability = 1
        return std::abs(a - b) / std::min(std::abs(a), std::abs(b));
    }
}

} // namespace glm

} // namespace modules

} // namespace madlib
