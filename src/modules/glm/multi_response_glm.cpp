/* ----------------------------------------------------------------------- *//**
 *
 * @file multi_response_glm.cpp
 *
 * @brief multivariate response Generalized linear model functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/prob/student.hpp>

#include "MultiResponseGLM_proto.hpp"
#include "MultiResponseGLM_impl.hpp"
#include "multi_response_glm.hpp"

namespace madlib {

namespace modules {

namespace glm {

// types of states
typedef MultiResponseGLMAccumulator<RootContainer> MultiResponseGLMState;
typedef MultiResponseGLMAccumulator<MutableRootContainer> MutableMultiResponseGLMState;

AnyType
multi_response_glm_multinom_logit_transition::run(AnyType& args) {
    MutableMultiResponseGLMState state = args[0].getAs<MutableByteString>();
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
                state.num_features * (state.num_categories-1));

        // MADLIB-667: GPDB limits the single array size to be 1GB, which means
        // that the size of a double array cannot be large than 134217727
        // because (134217727 * 8) / (1024 * 1024) = 1023. And solve
        // state_size = x^2 + 2^x + 6 <= 134217727 will give x <= 11584.
        uint32_t state_size = 6 +
                state.optimizer.num_coef * state.optimizer.num_coef +
                2 * state.optimizer.num_coef;
        if(state_size > 134217727){
            throw std::runtime_error(
                "The product of number of independent variables and number of "
                "categories cannot be larger than 11584.");
        }

        state.resize();
        if (!args[3].isNull()) {
            MultiResponseGLMState prev_state = args[3].getAs<ByteString>();
            state = prev_state;
            state.reset();
        }
    }
    state << MutableMultiResponseGLMState::tuple_type(x, y);
    return state.storage();
}

// ------------------------------------------------------------------------

AnyType
multi_response_glm_merge_states::run(AnyType& args) {
    MutableMultiResponseGLMState stateLeft = args[0].getAs<MutableByteString>();
    MultiResponseGLMState stateRight = args[1].getAs<ByteString>();

    stateLeft << stateRight;
    return stateLeft.storage();
}

AnyType
multi_response_glm_final::run(AnyType& args) {
    MutableMultiResponseGLMState state = args[0].getAs<MutableByteString>();

    // If we haven't seen any valid data, just return Null. This is the standard
    // behavior of aggregate function on empty data sets (compare, e.g.,
    // how PostgreSQL handles sum or avg on empty inputs)
    if (state.empty() || state.terminated) { return Null(); }

    state.apply();
    return state.storage();
}

// ------------------------------------------------------------------------

AnyType
multi_response_glm_result_z_stats::run(AnyType& args) {
    if (args[0].isNull()) { return Null(); }

    MultiResponseGLMState state = args[0].getAs<ByteString>();
    MultiResponseGLMResult result(state);

    AnyType tuple;
    tuple << result.coef
          << result.loglik
          << result.std_err
          << result.z_stats
          << result.p_values
          << result.num_rows_processed;

    return tuple;
}

// ------------------------------------------------------------------------

AnyType
multi_response_glm_loglik_diff::run(AnyType& args) {
    if (args[0].isNull() || args[1].isNull()) {
        return std::numeric_limits<double>::infinity();
    } else {
        MultiResponseGLMState state0 = args[0].getAs<ByteString>();
        MultiResponseGLMState state1 = args[1].getAs<ByteString>();
        double a = state0.loglik;
        double b = state1.loglik;
        if (a >= 0. || b >= 0.) { return 0.; } // probability = 1
        return std::abs(a - b) / std::min(std::abs(a), std::abs(b));
    }
}

} // namespace glm

} // namespace modules

} // namespace madlib
