/* ----------------------------------------------------------------------- *//**
 *
 * @file glm.cpp
 *
 * @brief Generalized linear model functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/prob/student.hpp>

#include "GLM_proto.hpp"
#include "GLM_impl.hpp"
#include "glm.hpp"

namespace madlib {

namespace modules {

namespace glm {

// types of states
typedef GLMAccumulator<RootContainer> GLMState;
typedef GLMAccumulator<MutableRootContainer> MutableGLMState;

#define DEFINE_GLM_TRANSITION(_state_type) \
    _state_type state = args[0].getAs<MutableByteString>(); \
    if (state.terminated || args[1].isNull() || args[2].isNull()) { \
        return args[0]; \
    } \
    double y = args[1].getAs<double>(); \
    MappedColumnVector x; \
    try { \
        MappedColumnVector xx = args[2].getAs<MappedColumnVector>(); \
        x.rebind(xx.memoryHandle(), xx.size()); \
    } catch (const ArrayWithNullException &e) { \
        return args[0]; \
    } \
 \
    if (state.empty()) { \
        state.num_features = static_cast<uint16_t>(x.size()); \
        state.resize(); \
        if (!args[3].isNull()) { \
            GLMState prev_state = args[3].getAs<ByteString>(); \
            state = prev_state; \
            state.reset(); \
        } \
    } \
 \
    state << MutableGLMState::tuple_type(x, y); \
    return state.storage()

// ------------------------------------------------------------------------

typedef GLMAccumulator<MutableRootContainer,Poisson,Log>
    MutableGLMPoissonLogState;

AnyType
glm_poisson_log_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMPoissonLogState);
}

// ------------------------------------------------------------------------

typedef GLMAccumulator<MutableRootContainer,Poisson,Identity>
    MutableGLMPoissonIdentityState;

AnyType
glm_poisson_identity_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMPoissonIdentityState);
}

// ------------------------------------------------------------------------

typedef GLMAccumulator<MutableRootContainer,Poisson,Sqrt>
    MutableGLMPoissonSqrtState;

AnyType
glm_poisson_sqrt_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMPoissonSqrtState);
}

// ------------------------------------------------------------

typedef GLMAccumulator<MutableRootContainer,Gaussian,Log>
    MutableGLMGaussianLogState;

AnyType
glm_gaussian_log_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMGaussianLogState);
}

// ------------------------------------------------------------------------

typedef GLMAccumulator<MutableRootContainer,Gaussian,Identity>
    MutableGLMGaussianIdentityState;

AnyType
glm_gaussian_identity_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMGaussianIdentityState);
}

// ------------------------------------------------------------------------

typedef GLMAccumulator<MutableRootContainer,Gaussian,Inverse>
    MutableGLMGaussianInverseState;

AnyType
glm_gaussian_inverse_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMGaussianInverseState);
}

// ------------------------------------------------------------------------

AnyType
glm_merge_states::run(AnyType& args) {
    MutableGLMState stateLeft = args[0].getAs<MutableByteString>();
    GLMState stateRight = args[1].getAs<ByteString>();

    stateLeft << stateRight;
    return stateLeft.storage();
}

AnyType
glm_final::run(AnyType& args) {
    MutableGLMState state = args[0].getAs<MutableByteString>();

    // If we haven't seen any valid data, just return Null. This is the standard
    // behavior of aggregate function on empty data sets (compare, e.g.,
    // how PostgreSQL handles sum or avg on empty inputs)
    if (state.empty() || state.terminated) { return Null(); }

    state.apply();
    return state.storage();
}

// ------------------------------------------------------------------------

AnyType
glm_result_z_stats::run(AnyType& args) {
    if (args[0].isNull()) { return Null(); }

    GLMState state = args[0].getAs<ByteString>();
    GLMResult result(state);

    AnyType tuple;
    tuple << result.coef
          << result.loglik
          << result.std_err
          << result.z_stats
          << result.p_values
          << result.num_rows_processed
          << result.dispersion;

    return tuple;
}


// ------------------------------------------------------------------------

AnyType
glm_result_t_stats::run(AnyType& args) {
    if (args[0].isNull()) { return Null(); }

    GLMState state = args[0].getAs<ByteString>();
    GLMResult result(state);

    result.std_err = result.std_err * sqrt(result.dispersion);
    result.z_stats = result.coef.cwiseQuotient(result.std_err);

    for (Index i = 0; i < state.num_features; i++) {
        result.p_values(i) = 2. * prob::cdf(
                boost::math::complement(
                    prob::students_t(
                        static_cast<double>(state.num_rows - state.num_features)
                        ),
                    std::fabs(result.z_stats(i))
                    ));
    }

    AnyType tuple;
    tuple << result.coef
          << result.loglik
          << result.std_err
          << result.z_stats
          << result.p_values
          << result.num_rows_processed
          << result.dispersion;

    return tuple;
}

// ------------------------------------------------------------------------

AnyType
glm_loglik_diff::run(AnyType& args) {
    if (args[0].isNull() || args[1].isNull()) {
        return std::numeric_limits<double>::infinity();
    } else {
        double a = GLMState(args[0].getAs<ByteString>()).loglik;
        double b = GLMState(args[1].getAs<ByteString>()).loglik;
        if (a >= 0. || b >= 0.) { return 0.; } // probability = 1
        return std::abs(a - b) / std::min(std::abs(a), std::abs(b));
    }
}

} // namespace glm

} // namespace modules

} // namespace madlib
