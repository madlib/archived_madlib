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
#include <cmath>

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
typedef GLMAccumulator<MutableRootContainer,Gamma,Log>
    MutableGLMGammaLogState;

AnyType
glm_gamma_log_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMGammaLogState);
}

// ------------------------------------------------------------------------

typedef GLMAccumulator<MutableRootContainer,Gamma,Identity>
    MutableGLMGammaIdentityState;

AnyType
glm_gamma_identity_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMGammaIdentityState);
}

// ------------------------------------------------------------------------

typedef GLMAccumulator<MutableRootContainer,Gamma,Inverse>
    MutableGLMGammaInverseState;

AnyType
glm_gamma_inverse_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMGammaInverseState);
}

// ------------------------------------------------------------

typedef GLMAccumulator<MutableRootContainer, Binomial, Probit>
    MutableGLMBinomialProbitState;

AnyType
glm_binomial_probit_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMBinomialProbitState);
}

// ------------------------------------------------------------

typedef GLMAccumulator<MutableRootContainer, Binomial, Logit>
    MutableGLMBinomialLogitState;

AnyType
glm_binomial_logit_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMBinomialLogitState);
}
// ------------------------------------------------------------------------
typedef GLMAccumulator<MutableRootContainer,Inverse_gaussian,Log>
    MutableGLMInverse_gaussianLogState;

AnyType
glm_inverse_gaussian_log_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMInverse_gaussianLogState);
}

// ------------------------------------------------------------------------

typedef GLMAccumulator<MutableRootContainer,Inverse_gaussian,Identity>
    MutableGLMInverse_gaussianIdentityState;

AnyType
glm_inverse_gaussian_identity_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMInverse_gaussianIdentityState);
}

// ------------------------------------------------------------------------

typedef GLMAccumulator<MutableRootContainer,Inverse_gaussian,Inverse>
    MutableGLMInverse_gaussianInverseState;

AnyType
glm_inverse_gaussian_inverse_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMInverse_gaussianInverseState);
}

// ------------------------------------------------------------------------
typedef GLMAccumulator<MutableRootContainer,Inverse_gaussian,Sqr_inverse>
    MutableGLMInverse_gaussianSqr_inverseState;

AnyType
glm_inverse_gaussian_sqr_inverse_transition::run(AnyType& args) {
    DEFINE_GLM_TRANSITION(MutableGLMInverse_gaussianSqr_inverseState);
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
          << 1.; // when z-stats is calculated, dispersion parameter is always 1

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

// ------------------------------------------------------------------------

AnyType glm_predict::run(AnyType &args) {
    try {
        args[0].getAs<MappedColumnVector>();
    } catch (const ArrayWithNullException &e) {
        throw std::runtime_error(
            "GLM error: the coefficients contain NULL values");
    }
    // returns NULL if args[1] (features) contains NULL values
    try {
        args[1].getAs<MappedColumnVector>();
    } catch (const ArrayWithNullException &e) {
        return Null();
    }

    MappedColumnVector vec1 = args[0].getAs<MappedColumnVector>();
    MappedColumnVector vec2 = args[1].getAs<MappedColumnVector>();
    char* link              = args[2].getAs<char*>();

    if (vec1.size() != vec2.size())
        throw std::runtime_error(
            "Coefficients and independent variables are of incompatible length");
    double dot = vec1.dot(vec2);

    if (strcmp(link,"identity")==0) return(dot);
    if (strcmp(link,"inverse")==0) return(1/dot);
    if (strcmp(link,"log")==0) return(exp(dot));
    if (strcmp(link,"sqrt")==0) return(dot*dot);
    if (strcmp(link,"sqr_inverse")==0) return(1/sqrt(dot));
    if (strcmp(link,"probit")==0) return(prob::cdf(prob::normal(), dot));
    if (strcmp(link,"logit")==0) return (1./(1 + exp(-dot)));

    throw std::runtime_error("Invalid link function!");
}

AnyType glm_predict_binomial::run(AnyType &args) {
    try {
        args[0].getAs<MappedColumnVector>();
    } catch (const ArrayWithNullException &e) {
        throw std::runtime_error(
            "GLM error: the coefficients contain NULL values");
    }
    // returns NULL if args[1] (features) contains NULL values
    try {
        args[1].getAs<MappedColumnVector>();
    } catch (const ArrayWithNullException &e) {
        return Null();
    }

    MappedColumnVector vec1 = args[0].getAs<MappedColumnVector>();
    MappedColumnVector vec2 = args[1].getAs<MappedColumnVector>();
    std::string link              = args[2].getAs<std::string>();
    if (vec1.size() != vec2.size())
        throw std::runtime_error(
            "Coefficients and independent variables are of incompatible length");
    double dot = vec1.dot(vec2);

    if (link.compare("probit")==0) return((prob::cdf(prob::normal(), dot)) >= 0.5);
    if (link.compare("logit")==0) return ((1./(1 + exp(-dot))) >= 0.5);

    throw std::runtime_error("Invalid link function!");
}

} // namespace glm

} // namespace modules

} // namespace madlib
