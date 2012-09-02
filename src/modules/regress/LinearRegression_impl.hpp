/* ----------------------------------------------------------------------- *//**
 *
 * @file LinearRegression_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_IMPL_HPP
#define MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_IMPL_HPP

#include <boost/math/distributions.hpp>
#include <modules/prob/student.hpp>

namespace madlib {

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
        ? 0
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
    else if (!isfinite(x))
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
    if (!isfinite(inState.X_transp_X) || !isfinite(inState.X_transp_Y))
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

} // namespace regress

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_REGRESS_LINEAR_REGRESSION_IMPL_HPP)
