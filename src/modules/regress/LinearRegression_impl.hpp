/* ----------------------------------------------------------------------- *//**
 *
 * @file LinearRegression_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_IMPL_HPP
#define MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_IMPL_HPP

#include <dbconnector/dbconnector.hpp>
#include <boost/math/distributions.hpp>
#include <modules/prob/student.hpp>
#include <modules/prob/boost.hpp>
#include <limits>

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

namespace regress {

template <class Container>
inline
LinearRegressionAccumulator<Container>::LinearRegressionAccumulator(
    Init_type& inInitialization)
  : Base(inInitialization) {

    this->initialize();
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
template <class Container>
inline
void
LinearRegressionAccumulator<Container>::bind(ByteStream_type& inStream) {
    inStream
        >> numRows >> widthOfX >> y_sum >> y_square_sum;
    uint16_t actualWidthOfX = widthOfX.isNull()
        ? static_cast<uint16_t>(0)
        : static_cast<uint16_t>(widthOfX);
    inStream
        >> X_transp_Y.rebind(actualWidthOfX)
        >> X_transp_X.rebind(actualWidthOfX, actualWidthOfX);
}

/**
 * @brief Update the accumulation state
 *
 * We update the number of rows \f$ n \f$, the partial
 * sums \f$ \sum_{i=1}^n y_i \f$ and \f$ \sum_{i=1}^n y_i^2 \f$, the matrix
 * \f$ X^T X \f$, and the vector \f$ X^T \boldsymbol y \f$.
 */
template <class Container>
inline
LinearRegressionAccumulator<Container>&
LinearRegressionAccumulator<Container>::operator<<(const tuple_type& inTuple) {
    const MappedColumnVector& x = std::get<0>(inTuple);
    const double& y = std::get<1>(inTuple);

    // The following checks were introduced with MADLIB-138. It still seems
    // useful to have clear error messages in case of infinite input values.
    if (!std::isfinite(y))
        throw std::domain_error("Dependent variables are not finite.");
    else if (!dbal::eigen_integration::isfinite(x))
        throw std::domain_error("Design matrix is not finite.");
    else if (x.size() > std::numeric_limits<uint16_t>::max())
        throw std::domain_error("Number of independent variables cannot be "
            "larger than 65535.");

    // Initialize in first iteration
    if (numRows == 0) {
        widthOfX = static_cast<uint16_t>(x.size());
        this->resize();
    } else if (widthOfX != static_cast<uint16_t>(x.size())) {
        throw std::runtime_error("Inconsistent numbers of independent "
            "variables.");
    }

    numRows++;
    y_sum += y;
    y_square_sum += y * y;
    X_transp_Y.noalias() += x * y;

    // X^T X is symmetric, so it is sufficient to only fill a triangular part
    // of the matrix
    triangularView<Lower>(X_transp_X) += x * trans(x);
    return *this;
}

/**
 * @brief Merge with another accumulation state
 */
template <class Container>
template <class OtherContainer>
inline
LinearRegressionAccumulator<Container>&
LinearRegressionAccumulator<Container>::operator<<(
    const LinearRegressionAccumulator<OtherContainer>& inOther) {

    // Initialize if necessary
    if (numRows == 0) {
        *this = inOther;
        return *this;
    } else if (inOther.numRows == 0)
        return *this;
    else if (widthOfX != inOther.widthOfX)
        throw std::runtime_error("Inconsistent numbers of independent "
            "variables.");

    numRows += inOther.numRows;
    y_sum += inOther.y_sum;
    y_square_sum += inOther.y_square_sum;
    X_transp_Y.noalias() += inOther.X_transp_Y;
    triangularView<Lower>(X_transp_X) += inOther.X_transp_X;
    return *this;
}

template <class Container>
template <class OtherContainer>
inline
LinearRegressionAccumulator<Container>&
LinearRegressionAccumulator<Container>::operator=(
    const LinearRegressionAccumulator<OtherContainer>& inOther) {

    this->copy(inOther);
    return *this;
}


template <class Container>
LinearRegression::LinearRegression(
    const LinearRegressionAccumulator<Container>& inState) {

    compute(inState);
}

/**
 * @brief Transform a linear-regression accumulation state into a result
 *
 * The result of the accumulation phase is \f$ X^T X \f$ and
 * \f$ X^T \boldsymbol y \f$. We first compute the pseudo-inverse, then the
 * regression coefficients, the model statistics, etc.
 *
 * @sa For the mathematical description, see \ref grp_linreg.
 */
template <class Container>
inline
LinearRegression&
LinearRegression::compute(
    const LinearRegressionAccumulator<Container>& inState) {

    Allocator& allocator = defaultAllocator();

    // The following checks were introduced with MADLIB-138. It still seems
    // useful to have clear error messages in case of infinite input values.
    if (!dbal::eigen_integration::isfinite(inState.X_transp_X) ||
            !dbal::eigen_integration::isfinite(inState.X_transp_Y))
        throw std::domain_error("Design matrix is not finite.");

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        inState.X_transp_X, EigenvaluesOnly, ComputePseudoInverse);

    // Precompute (X^T * X)^+
    Matrix inverse_of_X_transp_X = decomposition.pseudoInverse();
    conditionNo = decomposition.conditionNo();

    // Vector of coefficients: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    coef.rebind(allocator.allocateArray<double>(inState.widthOfX));
    coef.noalias() = inverse_of_X_transp_X * inState.X_transp_Y;

    // explained sum of squares (regression sum of squares)
    double ess = dot(inState.X_transp_Y, coef)
        - (inState.y_sum * inState.y_sum / static_cast<double>(inState.numRows));

    // total sum of squares
    double tss = inState.y_square_sum
        - (inState.y_sum * inState.y_sum / static_cast<double>(inState.numRows));

    // With infinite precision, the following checks are pointless. But due to
    // floating-point arithmetic, this need not hold at this point.
    // Without a formal proof convincing us of the contrary, we should
    // anticipate that numerical peculiarities might occur.
    if (tss < 0)
        tss = 0;
    if (ess < 0)
        ess = 0;
    // Since we know tss with greater accuracy than ess, we do the following
    // sanity adjustment to ess:
    if (ess > tss)
        ess = tss;

    // coefficient of determination
    // If tss == 0, then the regression perfectly fits the data, so the
    // coefficient of determination is 1.
    r2 = (tss == 0 ? 1 : ess / tss);

    // In the case of linear regression:
    // residual sum of squares (rss) = total sum of squares (tss) - explained
    // sum of squares (ess)
    // Proof: http://en.wikipedia.org/wiki/Sum_of_squares
    double rss = tss - ess;

    // Variance is also called the mean square error
    double variance = rss / static_cast<double>(inState.numRows - inState.widthOfX);

    // Vector of standard errors and t-statistics: For efficiency reasons, we
    // want to return these by reference, so we need to bind to db memory
    stdErr.rebind(allocator.allocateArray<double>(inState.widthOfX));
    tStats.rebind(allocator.allocateArray<double>(inState.widthOfX));
    for (int i = 0; i < inState.widthOfX; i++) {
        // In an abundance of caution, we see a tiny possibility that numerical
        // instabilities in the pinv operation can lead to negative values on
        // the main diagonal of even a SPD matrix
        if (inverse_of_X_transp_X(i,i) < 0) {
            stdErr(i) = 0;
        } else {
            stdErr(i) = std::sqrt( variance * inverse_of_X_transp_X(i,i) );
        }

        if (coef(i) == 0 && stdErr(i) == 0) {
            // In this special case, 0/0 should be interpreted as 0:
            // We know that 0 is the exact value for the coefficient, so
            // the t-value should be 0 (corresponding to a p-value of 1)
            tStats(i) = 0;
        } else {
            // If stdErr(i) == 0 then abs(tStats(i)) will be infinity, which
            // is what we need.
            tStats(i) = coef(i) / stdErr(i);
        }
    }

    // Matrix of variance-covariance matrix : For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    vcov.rebind(allocator.allocateArray<double>(inState.widthOfX,
                                                inState.widthOfX),
                inState.widthOfX,
                inState.widthOfX);
    vcov = variance * inverse_of_X_transp_X;

    // Vector of p-values: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    pValues.rebind(allocator.allocateArray<double>(inState.widthOfX));
    if (inState.numRows > inState.widthOfX)
        for (int i = 0; i < inState.widthOfX; i++)
            pValues(i) = 2. * prob::cdf(
                boost::math::complement(
                    prob::students_t(
                        static_cast<double>(inState.numRows - inState.widthOfX)
                    ),
                    std::fabs(tStats(i))
                ));
    return *this;
}

/*
 * Robust Linear Regression: Huber-White Sandwich estimator
 */

template <class Container>
inline
RobustLinearRegressionAccumulator<Container>::RobustLinearRegressionAccumulator(
    Init_type& inInitialization) : Base(inInitialization) {
    this->initialize();
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
template <class Container>
inline
void
RobustLinearRegressionAccumulator<Container>::bind(ByteStream_type& inStream) {
    inStream >> numRows >> widthOfX;
    uint16_t actualWidthOfX = widthOfX.isNull() ? static_cast<uint16_t>(0) : static_cast<uint16_t>(widthOfX);
    inStream
        >> ols_coef.rebind(actualWidthOfX)
        >> X_transp_X.rebind(actualWidthOfX, actualWidthOfX)
        >> X_transp_r2_X.rebind(actualWidthOfX, actualWidthOfX);
}

/**
 * @brief Update the accumulation state
 *
 * We update the number of rows \f$ n \f$, the matrix \f$ X^T X \f$,
 * and the matrix \f$ X^T diag(r_1^2, r_2^2 ... r_n^2 X \f$
 */
template <class Container>
inline
RobustLinearRegressionAccumulator<Container>&
RobustLinearRegressionAccumulator<Container>::operator<<(const tuple_type& inTuple) {

    // Inputs
    const MappedColumnVector& x = std::get<0>(inTuple);
    const double& y = std::get<1>(inTuple);
    const MappedColumnVector& coef = std::get<2>(inTuple);

    if (!std::isfinite(y))
        throw std::domain_error("Dependent variables are not finite.");
    else if (x.size() > std::numeric_limits<uint16_t>::max())
        throw std::domain_error("Number of independent variables cannot be "
            "larger than 65535.");

    // Initialize in first iteration
    if (numRows == 0) {
        widthOfX = static_cast<uint16_t>(x.size());
        this->resize();
        ols_coef = coef;
    }

    // dimension check
    if (widthOfX != static_cast<uint16_t>(x.size())) {
        throw std::runtime_error("Inconsistent numbers of independent "
            "variables.");
    }

    numRows++;
    double r = y - trans(ols_coef)*x;

    // The following matrices are symmetric, so it is sufficient to
    // only fill a triangular part
    triangularView<Lower>(X_transp_X) += x * trans(x);
    triangularView<Lower>(X_transp_r2_X) += r * r * x * trans(x);

    return *this;
}

/**
 * @brief Merge with another accumulation state
 */
template <class Container>
template <class OtherContainer>
inline
RobustLinearRegressionAccumulator<Container>&
RobustLinearRegressionAccumulator<Container>::operator<<(
    const RobustLinearRegressionAccumulator<OtherContainer>& inOther) {

    numRows += inOther.numRows;
    triangularView<Lower>(X_transp_X) += inOther.X_transp_X;
    triangularView<Lower>(X_transp_r2_X) += inOther.X_transp_r2_X;
    return *this;
}

template <class Container>
template <class OtherContainer>
inline
RobustLinearRegressionAccumulator<Container>&
RobustLinearRegressionAccumulator<Container>::operator=(
    const RobustLinearRegressionAccumulator<OtherContainer>& inOther) {

    this->copy(inOther);
    return *this;
}


template <class Container>
RobustLinearRegression::RobustLinearRegression(
    const RobustLinearRegressionAccumulator<Container>& inState) {

    compute(inState);
}

/**
 * @brief Transform a robust linear-regression accumulation state into a result
 *
 * The result of the accumulation phase is \f$ X^T X \f$ and
 * \f$ X^T U X \f$. We first compute the pseudo-inverse, then the
 * the robust model statistics.
 *
 * @sa For the mathematical description, see \ref grp_linreg.
 */
template <class Container>
inline
RobustLinearRegression&
RobustLinearRegression::compute(
    const RobustLinearRegressionAccumulator<Container>& inState) {

    Allocator& allocator = defaultAllocator();

    // The following checks were introduced with MADLIB-138. It still seems
    // useful to have clear error messages in case of infinite input values.
    if (!dbal::eigen_integration::isfinite(inState.X_transp_X) ||
            !dbal::eigen_integration::isfinite(inState.X_transp_r2_X))
        throw std::domain_error("Design matrix is not finite.");

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        inState.X_transp_X, EigenvaluesOnly, ComputePseudoInverse);

    // Precompute (X^T * X)^+
    Matrix inverse_of_X_transp_X = decomposition.pseudoInverse();

    // Calculate the robust variance covariance matrix as:
    // (X^T X)^-1  X^T diag(r1^2,r2^2....rn^2)X  (X^T X)^-1
    // Where r_1, r_2 ... r_n are the residuals
    // Note: X_transp_r2_X calcualtes X^T diag(r1^2,r2^2....rn^2)X

    Matrix robust_var_cov = inState.X_transp_r2_X.template triangularView<Eigen::StrictlyLower>();
    robust_var_cov = robust_var_cov + trans(inState.X_transp_r2_X);
    robust_var_cov = inverse_of_X_transp_X * robust_var_cov * inverse_of_X_transp_X;

    // Vector of standard errors and t-statistics: For efficiency reasons, we
    // want to return these by reference, so we need to bind to db memory
    coef.rebind(allocator.allocateArray<double>(inState.widthOfX));
    stdErr.rebind(allocator.allocateArray<double>(inState.widthOfX));
    tStats.rebind(allocator.allocateArray<double>(inState.widthOfX));
    for (int i = 0; i < inState.widthOfX; i++) {
        // In an abundance of caution, we see a tiny possibility that numerical
        // instabilities in the pinv operation can lead to negative values on
        // the main diagonal of even a SPD matrix
        coef(i) = inState.ols_coef(i);

        if (inverse_of_X_transp_X(i,i) < 0) {
            stdErr(i) = 0;
        } else {
            stdErr(i) = std::sqrt(robust_var_cov(i,i));
        }

        if (inState.ols_coef(i) == 0 && stdErr(i) == 0) {
            // In this special case, 0/0 should be interpreted as 0:
            // We know that 0 is the exact value for the coefficient, so
            // the t-value should be 0 (corresponding to a p-value of 1)
            tStats(i) = 0;
        } else {
            // If stdErr(i) == 0 then abs(tStats(i)) will be infinity, which
            // is what we need.
            tStats(i) = inState.ols_coef(i) / stdErr(i);
        }
    }

    // Vector of p-values: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    pValues.rebind(allocator.allocateArray<double>(inState.widthOfX));
    if (inState.numRows > inState.widthOfX){
        for (int i = 0; i < inState.widthOfX; i++){
            pValues(i) = 2. * prob::cdf(
                boost::math::complement(
                    prob::students_t(
                        static_cast<double>(inState.numRows - inState.widthOfX)
                    ),
                    std::fabs(tStats(i))
                ));
        }
    }
    return *this;
}


/*
  Regression for the tests for heteroskedasticity
*/

template <class Container>
inline
HeteroLinearRegressionAccumulator<Container>::HeteroLinearRegressionAccumulator(
    Init_type& inInitialization)
  : Base(inInitialization) {

    this->initialize();
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
template <class Container>
inline
void
HeteroLinearRegressionAccumulator<Container>::bind(ByteStream_type& inStream) {
    inStream
        >> numRows >> widthOfX >> a_sum >> a_square_sum;
    uint16_t actualWidthOfX = widthOfX.isNull()
        ? static_cast<uint16_t>(0)
        : static_cast<uint16_t>(widthOfX);
    inStream
        >> X_transp_A.rebind(actualWidthOfX)
        >> X_transp_X.rebind(actualWidthOfX, actualWidthOfX);
}

/**
 * @brief Update the accumulation state
 *
 * We update the number of rows \f$ n \f$, the partial
 * sums \f$ \sum_{i=1}^n y_i \f$ and \f$ \sum_{i=1}^n y_i^2 \f$, the matrix
 * \f$ X^T X \f$, and the vector \f$ X^T \boldsymbol y \f$.
 */
template <class Container>
inline
HeteroLinearRegressionAccumulator<Container>&
HeteroLinearRegressionAccumulator<Container>::operator<<(const hetero_tuple_type& inTuple) {
    const MappedColumnVector& x = std::get<0>(inTuple);
    const double& y = std::get<1>(inTuple);
    const MappedColumnVector& coef = std::get<2>(inTuple);

    if (!std::isfinite(y))
        throw std::domain_error("Dependent variables are not finite.");
    else if (!dbal::eigen_integration::isfinite(x))
        throw std::domain_error("Design matrix is not finite.");
    else if (x.size() > std::numeric_limits<uint16_t>::max())
        throw std::domain_error("Number of independent variables cannot be "
            "larger than 65535.");

    // Initialize in first iteration
    if (numRows == 0) {
        widthOfX = static_cast<uint16_t>(x.size());
        this->resize();
    }

    // dimension check
    if (widthOfX != static_cast<uint16_t>(x.size())) {
        throw std::runtime_error("Inconsistent numbers of independent "
            "variables.");
    }

    double a = y - trans(coef)*x;
    a = a*a;

    numRows++;
    a_sum += a;
    a_square_sum += a*a;
    X_transp_A.noalias() += x * a;

    // X^T X is symmetric, so it is sufficient to only fill a triangular part
    // of the matrix
    triangularView<Lower>(X_transp_X) += x * trans(x);
    return *this;
}

/**
 * @brief Merge with another accumulation state
 */
template <class Container>
template <class OtherContainer>
inline
HeteroLinearRegressionAccumulator<Container>&
HeteroLinearRegressionAccumulator<Container>::operator<<(
    const HeteroLinearRegressionAccumulator<OtherContainer>& inOther) {

    numRows += inOther.numRows;
    a_sum += inOther.a_sum;
    a_square_sum += inOther.a_square_sum;
    X_transp_A.noalias() += inOther.X_transp_A;
    triangularView<Lower>(X_transp_X) += inOther.X_transp_X;
    return *this;
}

template <class Container>
template <class OtherContainer>
inline
HeteroLinearRegressionAccumulator<Container>&
HeteroLinearRegressionAccumulator<Container>::operator=(
    const HeteroLinearRegressionAccumulator<OtherContainer>& inOther) {

    this->copy(inOther);
    return *this;
}


template <class Container>
HeteroLinearRegression::HeteroLinearRegression(
    const HeteroLinearRegressionAccumulator<Container>& inState) {

    compute(inState);
}

/**
 * @brief Transform a linear-regression accumulation state into a result
 *
 * The result of the accumulation phase is \f$ X^T X \f$ and
 * \f$ X^T \boldsymbol y \f$. We first compute the pseudo-inverse, then the
 * regression coefficients, the model statistics, etc.
 *
 * @sa For the mathematical description, see \ref grp_linreg.
 */
template <class Container>
inline
HeteroLinearRegression&
HeteroLinearRegression::compute(
    const HeteroLinearRegressionAccumulator<Container>& inState) {

    // The following checks were introduced with MADLIB-138. It still seems
    // useful to have clear error messages in case of infinite input values.
    if (!dbal::eigen_integration::isfinite(inState.X_transp_X) ||
            !dbal::eigen_integration::isfinite(inState.X_transp_A))
        throw std::domain_error("Design matrix is not finite.");

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        inState.X_transp_X, EigenvaluesOnly, ComputePseudoInverse);

    // Precompute (X^T * X)^+
    Matrix inverse_of_X_transp_X = decomposition.pseudoInverse();

    ColumnVector coef;
    coef = inverse_of_X_transp_X * inState.X_transp_A;

    // explained sum of squares (regression sum of squares)
    double ess = dot(inState.X_transp_A, coef)
        - (inState.a_sum * inState.a_sum / static_cast<double>(inState.numRows));

    // total sum of squares
    double tss = inState.a_square_sum
        - (inState.a_sum * inState.a_sum / static_cast<double>(inState.numRows));

    // With infinite precision, the following checks are pointless. But due to
    // floating-point arithmetic, this need not hold at this point.
    // Without a formal proof convincing us of the contrary, we should
    // anticipate that numerical peculiarities might occur.
    if (tss < 0) tss = 0;
    if (ess < 0) ess = 0;
    // Since we know tss with greater accuracy than ess, we do the following
    // sanity adjustment to ess:
    if (ess > tss) ess = tss;

    // Test statistic: numRows*Coefficient of determination
    test_statistic = static_cast<double>(inState.numRows) * (tss == 0 ? 1 : ess / tss);
    pValue = prob::cdf(complement(prob::chi_squared(
                                      static_cast<double>(inState.widthOfX-1)), test_statistic));

    return *this;
}

} // namespace regress

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_IMPL_HPP)
