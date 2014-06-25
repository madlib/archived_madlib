/* ----------------------------------------------------------------------- *//**
 *
 * @file GLM_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_GLM_GLM_IMPL_HPP
#define MADLIB_MODULES_GLM_GLM_IMPL_HPP

#include <dbconnector/dbconnector.hpp>
#include <boost/math/distributions.hpp>
#include <modules/prob/boost.hpp>
#include <limits>

namespace madlib {

namespace modules {

namespace glm {

// Use Eigen
using namespace dbal::eigen_integration;

template <class Container, class Family, class Link>
inline
GLMAccumulator<Container,Family,Link>::GLMAccumulator(
        Init_type& inInitialization)
  : Base(inInitialization) {

    this->initialize();
}


/**
 * @brief Reset the accumulator before accumulating the first tuple
 */
template <class Container, class Family, class Link>
inline
void
GLMAccumulator<Container,Family,Link>::reset() {
    num_rows = 0;
    terminated = false;
    loglik = 0.;
    X_trans_W_Y.fill(0);
    X_trans_W_X.fill(0);
    dispersion_accum = 0.;
}

/**
 * @brief Bind all elements of the state to the data in the stream
 *
 * The bind() is special in that even after running operator>>() on an element,
 * there is no guarantee yet that the element can indeed be accessed. It is
 * cruicial to first check this.
 *
 * Provided that this methods correctly lists all member variables, all other
 * methods can, however, rely on that fact that all variables are correctly
 * initialized and accessible.
 */
template <class Container, class Family, class Link>
inline
void
GLMAccumulator<Container,Family,Link>::bind(
        ByteStream_type& inStream) {
    inStream >> num_features;
    uint16_t actual_num_features = num_features.isNull()
        ? static_cast<uint16_t>(0)
        : static_cast<uint16_t>(num_features);
    inStream
        >> beta.rebind(actual_num_features)
        >> terminated
        >> loglik
        >> num_rows
        >> dispersion
        >> dispersion_accum
        >> X_trans_W_Y.rebind(actual_num_features)
        >> X_trans_W_X.rebind(actual_num_features, actual_num_features);

    // bind xcov and X_trans_W_X onto the same memory as we don't need them
    // at the same time
    xcov.rebind(X_trans_W_X.memoryHandle(),
                X_trans_W_X.rows(),
                X_trans_W_X.cols());
}

/**
 * @brief Update the accumulation state by feeding a tuple
 */
template <class Container, class Family, class Link>
inline
GLMAccumulator<Container,Family,Link>&
GLMAccumulator<Container,Family,Link>::operator<<(
        const tuple_type& inTuple) {
    const MappedColumnVector& x = std::get<0>(inTuple);
    const double& y = std::get<1>(inTuple);

    // The following checks were introduced with MADLIB-138. It still seems
    // useful to have clear error messages in case of infinite input values.
    if (!std::isfinite(y)) {
        warning("Dependent variables are not finite.");
    } else if (!dbal::eigen_integration::isfinite(x)) {
        warning("Design matrix is not finite.");
    } else if (x.size() > std::numeric_limits<uint16_t>::max()) {
        warning("Number of independent variables cannot be "
            "larger than 65535.");
    } else if (num_features != static_cast<uint16_t>(x.size())) {
        warning("Inconsistent numbers of independent variables.");
    } else {
        // normal case
        if (beta.norm() == 0.) {
            double mu = Link::init(y);
            double ita = Link::link_func(mu);
            double G_prime = Link::mean_derivative(ita);
            double V = Family::variance(mu);
            double w = G_prime * G_prime / V;
            // Note: we need to remove the placehoder for normal, gamma, etc.
            dispersion_accum += (y - mu) * (y - mu) / V;
            loglik += Family::loglik(y, mu, dispersion);
            X_trans_W_X += x * trans(x) * w;
            X_trans_W_Y += x * w * ita;
        } else {
            double ita = trans(x) * beta;
            double mu = Link::mean_func(ita);
            double G_prime = Link::mean_derivative(ita);
            double V = Family::variance(mu);
            double w = G_prime * G_prime / V;
            // Note: we need to remove the placehoder for normal, gamma, etc.
            dispersion_accum += (y - mu) * (y - mu) / V;
            loglik += Family::loglik(y, mu, dispersion);
            X_trans_W_X += x * trans(x) * w;
            X_trans_W_Y += x * (y - mu) * G_prime / V;
        }
        num_rows ++;
        return *this;
    }

    // error case
    terminated = true;
    return *this;
}

/**
 * @brief Merge with another accumulation state
 */
template <class Container, class Family, class Link>
template <class C, class F, class L>
inline
GLMAccumulator<Container,Family,Link>&
GLMAccumulator<Container,Family,Link>::operator<<(
        const GLMAccumulator<C,F,L>& inOther) {
    if (this->empty()) {
        *this = inOther;
    } else if (inOther.empty()) {
    } else if (num_features != inOther.num_features) {
        warning("Inconsistent numbers of independent variables.");
        terminated = true;
    } else {
        num_rows += inOther.num_rows;
        loglik += inOther.loglik;
        X_trans_W_Y += inOther.X_trans_W_Y;
        X_trans_W_X += inOther.X_trans_W_X;
        dispersion_accum += inOther.dispersion_accum;
    }

    return *this;
}

/**
 * @brief Copy from a previous state
 */
template <class Container, class Family, class Link>
template <class C, class F, class L>
inline
GLMAccumulator<Container,Family,Link>&
GLMAccumulator<Container,Family,Link>::operator=(
        const GLMAccumulator<C,F,L>& inOther) {
    this->copy(inOther);
    return *this;
}

/**
 * @brief Apply the accumulated intra-state values to inter-state member
 */
template <class Container, class Family, class Link>
inline
void
GLMAccumulator<Container,Family,Link>::apply() {
    if (!dbal::eigen_integration::isfinite(X_trans_W_X) ||
            !dbal::eigen_integration::isfinite(X_trans_W_Y)) {
        warning("Hessian or gradient is not finite.");
        terminated = true;
    } else {
        xcov = X_trans_W_X.inverse();
        beta += xcov * X_trans_W_Y;
        dispersion = dispersion_accum / num_rows;
    }
}

// -----------------------------------------------------------------------

template <class Container>
GLMResult::GLMResult(const GLMAccumulator<Container>& state) {
    compute(state);
}

/**
 * @brief Transform an accumulation state into a result
 */
template <class Container>
inline
GLMResult&
GLMResult::compute(const GLMAccumulator<Container>& state) {
    // Vector of coefficients: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    Allocator& allocator = defaultAllocator();
    coef.rebind(allocator.allocateArray<double>(state.num_features));
    std_err.rebind(allocator.allocateArray<double>(state.num_features));
    z_stats.rebind(allocator.allocateArray<double>(state.num_features));
    p_values.rebind(allocator.allocateArray<double>(state.num_features));

    loglik = state.loglik;
    coef = state.beta;
    dispersion = state.dispersion * state.num_rows / (state.num_rows - state.num_features);
    std_err = state.xcov.diagonal().cwiseSqrt();
    z_stats = coef.cwiseQuotient(std_err);
    for (Index i = 0; i < state.num_features; i ++) {
        p_values(i) = 2. * prob::cdf( prob::normal(), -std::abs(z_stats(i)));
    }
    num_rows_processed = state.num_rows;

    return *this;
}

} // namespace glm

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_GLM_GLM_IMPL_HPP)
