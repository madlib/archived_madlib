/* ----------------------------------------------------------------------- *//**
 *
 * @file MultiResponseGLM_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_GLM_MULTIRESPONSEGLM_IMPL_HPP
#define MADLIB_MODULES_GLM_MULTIRESPONSEGLM_IMPL_HPP

#include <dbconnector/dbconnector.hpp>
#include <boost/math/distributions.hpp>
#include <limits>

namespace madlib {

namespace modules {

namespace glm {

// Use Eigen
using namespace dbal::eigen_integration;

template <class Container, class Family, class Link>
inline
MultiResponseGLMAccumulator<Container,Family,Link>::MultiResponseGLMAccumulator(
        Init_type& inInitialization)
  : Base(inInitialization), optimizer(inInitialization) {

    this->initialize();
}


/**
 * @brief Reset the accumulator before accumulating the first tuple
 */
template <class Container, class Family, class Link>
inline
void
MultiResponseGLMAccumulator<Container,Family,Link>::reset() {
    num_rows = 0;
    terminated = false;
    loglik = 0.;
    optimizer.reset();
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
MultiResponseGLMAccumulator<Container,Family,Link>::bind(
        ByteStream_type& inStream) {

    inStream
        >> num_features
        >> num_categories
        >> num_rows
        >> terminated
        >> loglik
        >> optimizer;

    // bind vcov and optimizer.hessian onto the same memory as we don't need them
    // at the same time
    vcov.rebind(optimizer.hessian.memoryHandle(),
                optimizer.hessian.rows(),
                optimizer.hessian.cols());
}

/**
 * @brief Update the accumulation state by feeding a tuple
 */
template <class Container, class Family, class Link>
inline
MultiResponseGLMAccumulator<Container,Family,Link>&
MultiResponseGLMAccumulator<Container,Family,Link>::operator<<(
        const tuple_type& inTuple) {

    const MappedColumnVector& x = std::get<0>(inTuple);
    const int& y = static_cast<int>(std::get<1>(inTuple));
    // convert y to an indicator vector
    ColumnVector vecY(num_categories-1);
    vecY.fill(0);
    if (y != 0) { vecY(y - 1) = 1; }

    // The following checks were introduced with MADLIB-138. It still seems
    // useful to have clear error messages in case of infinite input values.
    if (!dbal::eigen_integration::isfinite(x)) {
        warning("Design matrix is not finite.");
    } else if (x.size() > std::numeric_limits<uint16_t>::max()) {
        warning("Number of independent variables cannot be "
            "larger than 65535.");
    } else if (num_features != static_cast<uint16_t>(x.size())) {
        warning("Inconsistent numbers of independent variables.");
    } else {
        // normal case
        uint16_t N = num_features;
        uint16_t C = static_cast<uint16_t>(num_categories - 1);
        ColumnVector mu(C);
        ColumnVector ita(C);
        Matrix G_prime(C, C);
        Matrix V(C, C);

        // initialize ita and mu in the first iteration
        if (optimizer.beta.norm() == 0.) {
            Link::init(mu);
            Link::link_func(mu,ita);
        } else {
            for (int i = 0; i < ita.size(); i ++) {
                ita(i) = trans(x) * optimizer.beta.segment(i*N, N);
            }
            Link::mean_func(ita, mu);
        }

        // intermediate values
        Link::mean_derivative(ita, G_prime);
        Family::variance(mu, V);

        // log-likelihood
        loglik += Family::loglik(vecY, mu, 1.);

        // calcualte X_trans_W_X
        Matrix GtVinvG(trans(G_prime) * V.inverse() * G_prime);
        Matrix XXt(x * trans(x));
        for (int i = 0; i < C; i ++) {
            for (int j = 0; j < C; j ++) {
                for (int x_i = 0; x_i < num_features; x_i ++) {
                   for (int x_j = 0; x_j < num_features; x_j ++) {
                        optimizer.hessian(i*num_features + x_i,
                                          j*num_features + x_j)
                                += GtVinvG(i,j) * XXt(x_i,x_j);
                   }
                }
            }
        }

        // calculate X_trans_W_Y
        ColumnVector YVinvG(trans(vecY-mu) * V.inverse() * G_prime);
        for (int i = 0; i < C; i ++) {
            optimizer.grad.segment(i*N, N) -= YVinvG(i) * x;
        }
        num_rows ++;
        return *this;
    } // all tests passed and we were in the else

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
MultiResponseGLMAccumulator<Container,Family,Link>&
MultiResponseGLMAccumulator<Container,Family,Link>::operator<<(
        const MultiResponseGLMAccumulator<C,F,L>& inOther) {
    if (this->empty()) {
        *this = inOther;
    } else if (inOther.empty()) {
    } else if (num_features != inOther.num_features) {
        warning("Inconsistent numbers of independent variables.");
        terminated = true;
    } else {
        num_rows += inOther.num_rows;
        loglik += inOther.loglik;
        optimizer.grad += inOther.optimizer.grad;
        optimizer.hessian += inOther.optimizer.hessian;
    }

    return *this;
}

/**
 * @brief Copy from a previous state
 */
template <class Container, class Family, class Link>
template <class C, class F, class L>
inline
MultiResponseGLMAccumulator<Container,Family,Link>&
MultiResponseGLMAccumulator<Container,Family,Link>::operator=(
        const MultiResponseGLMAccumulator<C,F,L>& inOther) {
    this->copy(inOther);
    return *this;
}

/**
 * @brief Apply the accumulated intra-state values to inter-state member
 */
template <class Container, class Family, class Link>
inline
void
MultiResponseGLMAccumulator<Container,Family,Link>::apply() {
    if (!dbal::eigen_integration::isfinite(optimizer.hessian) ||
            !dbal::eigen_integration::isfinite(optimizer.grad)) {
        warning("Hessian or gradient is not finite.");
        terminated = true;
    } else {
        optimizer.apply();
    }

}
// -----------------------------------------------------------------------

template <class Container>
MultiResponseGLMResult::MultiResponseGLMResult(const MultiResponseGLMAccumulator<Container>& state) {
    compute(state);
}

/**
 * @brief Transform an accumulation state into a result
 */
template <class Container>
inline
MultiResponseGLMResult&
MultiResponseGLMResult::compute(const MultiResponseGLMAccumulator<Container>& state) {
    typedef Eigen::Map<Matrix> MMap;
    typedef MMap::Scalar Scalar;
    // Vector of coefficients: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    Allocator& allocator = defaultAllocator();
    Index N = static_cast<Index>(state.num_features);
    Index C = static_cast<Index>(state.num_categories - 1);
    coef.rebind(allocator.allocateArray<double>(C, N), N, C);
    std_err.rebind(allocator.allocateArray<double>(C, N), N, C);
    z_stats.rebind(allocator.allocateArray<double>(C, N), N, C);
    p_values.rebind(allocator.allocateArray<double>(C, N), N, C);

    // computation
    loglik = state.loglik;
    coef = MMap(const_cast<Scalar*>(state.optimizer.beta.data()), N, C);
    Matrix tmp_sd(state.vcov.diagonal().cwiseSqrt());
    tmp_sd.resize(N, C);
    std_err = tmp_sd;
    z_stats = coef.cwiseQuotient(std_err);
    for (Index i = 0; i < N*C; i ++) {
        p_values(i) = 2. * prob::cdf( prob::normal(), -std::abs(z_stats(i)));
    }
    num_rows_processed = state.num_rows;

    return *this;
}

} // namespace glm

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_GLM_MULTIRESPONSEGLM_IMPL_HPP)
