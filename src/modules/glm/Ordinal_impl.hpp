/* ----------------------------------------------------------------------- *//**
 *
 * @file Ordinal_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_GLM_ORDINAL_IMPL_HPP
#define MADLIB_MODULES_GLM_ORDINAL_IMPL_HPP

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
OrdinalAccumulator<Container,Family,Link>::OrdinalAccumulator(
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
OrdinalAccumulator<Container,Family,Link>::reset() {
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
OrdinalAccumulator<Container,Family,Link>::bind(
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
OrdinalAccumulator<Container,Family,Link>&
OrdinalAccumulator<Container,Family,Link>::operator<<(
        const tuple_type& inTuple) {

    const MappedColumnVector& x = std::get<0>(inTuple);
    const int& y = static_cast<int>(std::get<1>(inTuple));
    // convert y to an indicator vector
    ColumnVector vecY(num_categories-1);
    vecY.fill(0);
    if (y != (num_categories-1)) { vecY(y) = 1; }

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

        uint32_t state_size = 6 + (N + C)*(N + C) + 2 * (N + C);
        // GPDB limits the single array size to be 1GB, which means that the size
        // of a double array cannot be large than 134217727 because
        // (134217727 * 8) / (1024 * 1024) = 1023. And solve
        // state_size = x^2 + 2^x + 6 <= 134217727 will give x <= 11584.
        if(state_size > 134217727)
            throw std::domain_error(
                "The sum of number of independent variables and number of "
                 "categories cannot be larger than 11584.");
        ColumnVector mu(C);
        ColumnVector ita(C);
        Matrix G_prime(C, C);
        Matrix V(C, C);

        if (optimizer.beta.norm() == 0.) {
            // beta is 0 in the first iteration
            Link::init(mu);
            Link::link_func(mu,ita);
        } else {
            //beta has non-zero value
            double Xtbeta = trans(x)*optimizer.beta.segment(C,N);
            for (int i=0; i<ita.size(); i++) 
                ita(i) = optimizer.beta(i) - Xtbeta;
            Link::mean_func(ita, mu);
       }

        Link::mean_derivative(ita, G_prime);
        Family::variance(mu, V);
        loglik += Family::loglik(vecY, mu, 1);
        Matrix Vinv(V.inverse());
        
        // calcualte X_trans_W_X
        Matrix GtVinvG(trans(G_prime)*Vinv*G_prime);
        ColumnVector h_vec(C);
        h_vec.fill(0);
        for (int i=0;i<C;i++)
            for (int j=0;j<C;j++)
                 h_vec(i) += G_prime(i,j);
        
        Matrix GtVinvhXt(trans(G_prime)*Vinv*h_vec*trans(x));
        GtVinvhXt = -1*GtVinvhXt;
        Matrix XhtVinvG(x*trans(h_vec)*Vinv*G_prime);
        XhtVinvG = -1*XhtVinvG;
        Matrix htVinvhXXt(x*trans(x));
        double temp  = trans(h_vec)*Vinv*h_vec;
        htVinvhXXt = temp * htVinvhXXt;

        for (int i=0;i<C;i++) 
            for (int j=0;j<C;j++)
                optimizer.hessian(i,j) += GtVinvG(i,j);
        for (int i=0;i<C;i++)
            for (int j=C;j<(C+N);j++)
                optimizer.hessian(i,j) += GtVinvhXt(i,j-C);

        for (int i=C;i<(C+N);i++)
            for (int j=0;j<C;j++)
                optimizer.hessian(i,j) += XhtVinvG(i-C,j);

        for (int i=C;i<(C+N);i++)
            for (int j=C;j<(C+N);j++)
                optimizer.hessian(i,j) += htVinvhXXt(i-C,j-C);

        // calculate X_trans_W_Y
        ColumnVector YVinvG(trans(vecY - mu)*Vinv*G_prime);
        ColumnVector YVinvhX(x);
        temp = trans(vecY - mu)*Vinv*h_vec;
        YVinvhX = -1*temp * YVinvhX;

        optimizer.grad.segment(0, C) += YVinvG;
        optimizer.grad.segment(C, N) += YVinvhX;

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
OrdinalAccumulator<Container,Family,Link>&
OrdinalAccumulator<Container,Family,Link>::operator<<(
        const OrdinalAccumulator<C,F,L>& inOther) {
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
OrdinalAccumulator<Container,Family,Link>&
OrdinalAccumulator<Container,Family,Link>::operator=(
        const OrdinalAccumulator<C,F,L>& inOther) {
    this->copy(inOther);
    return *this;
}

/**
 * @brief Apply the accumulated intra-state values to inter-state member
 */
template <class Container, class Family, class Link>
inline
void
OrdinalAccumulator<Container,Family,Link>::apply() {
    if (!dbal::eigen_integration::isfinite(optimizer.hessian) ||
            !dbal::eigen_integration::isfinite(optimizer.grad)) {
        warning("Ordinal:Hessian or gradient is not finite. One possibility is that intercept is included in the independent variables. If that is the case, please drop the intercept and rerun the function.");
        terminated = true;
    } else {
                vcov = optimizer.hessian.inverse();
                optimizer.beta += vcov * optimizer.grad;
    }

}
// -----------------------------------------------------------------------

template <class Container>
OrdinalResult::OrdinalResult(const OrdinalAccumulator<Container>& state) {
    compute(state);
}

/**
 * @brief Transform an accumulation state into a result
 */
template <class Container>
inline
OrdinalResult&
OrdinalResult::compute(const OrdinalAccumulator<Container>& state) {
    // Vector of coefficients: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    Allocator& allocator = defaultAllocator();
    Index N = static_cast<Index>(state.num_features);
    Index C = static_cast<Index>(state.num_categories - 1);
    
    coef_alpha.rebind(allocator.allocateArray<double>(C));
    std_err_alpha.rebind(allocator.allocateArray<double>(C));
    z_stats_alpha.rebind(allocator.allocateArray<double>(C));
    p_values_alpha.rebind(allocator.allocateArray<double>(C));
    
    coef_beta.rebind(allocator.allocateArray<double>(N));
    std_err_beta.rebind(allocator.allocateArray<double>(N));
    z_stats_beta.rebind(allocator.allocateArray<double>(N));
    p_values_beta.rebind(allocator.allocateArray<double>(N));

    // computation
    loglik = state.loglik;
    coef_alpha = state.optimizer.beta.segment(0,C);
    ColumnVector temp(state.vcov.diagonal().cwiseSqrt());
    std_err_alpha = temp.segment(0,C);
    z_stats_alpha = coef_alpha.cwiseQuotient(std_err_alpha);
    for (Index i = 0; i < C; i ++) {
        p_values_alpha(i) = 2. * prob::cdf( prob::normal(), -std::abs(z_stats_alpha(i)));
    }
    
    coef_beta = state.optimizer.beta.segment(C,N);
    std_err_beta = temp.segment(C,N);
    z_stats_beta = coef_beta.cwiseQuotient(std_err_beta);
    for (Index i = 0; i < N; i ++) {
        p_values_beta(i) = 2. * prob::cdf( prob::normal(), -std::abs(z_stats_beta(i)));
    }
    num_rows_processed = state.num_rows;

    return *this;
}

} // namespace glm

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_GLM_ORDINAL_IMPL_HPP)
