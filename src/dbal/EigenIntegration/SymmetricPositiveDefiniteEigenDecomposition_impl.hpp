/* ----------------------------------------------------------------------- *//**
 *
 * @file SymmetricPositiveDefiniteEigenDecomposition_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_EIGEN_INTEGRATION_SPDEIGENDECOMPOSITION_IMPL_HPP
#define MADLIB_DBAL_EIGEN_INTEGRATION_SPDEIGENDECOMPOSITION_IMPL_HPP

namespace madlib {

namespace dbal {

namespace eigen_integration {

/**
 * @brief Constructor that invokes the computation
 *
 * @param inMatrix Matrix to operate on. Note that the type is
 *     <tt>const MatrixType&</tt>, meaning that a temporary object will be
 *     created if the actual parameter is not of a subclass type. This means
 *     that memory will be copied -- on the positive side, this ensures memory
 *     will be aligned!
 * @param inOptions A combination of DecompositionOptions
 * @param inExtras A combination of SPDDecompositionExtras
 */
template <class MatrixType>
inline
SymmetricPositiveDefiniteEigenDecomposition<MatrixType>
    ::SymmetricPositiveDefiniteEigenDecomposition(
    const MatrixType &inMatrix, int inOptions, int inExtras)
  : Base(inMatrix, inOptions) {

    computeExtras(inMatrix, inExtras);
}

/**
 * @brief Return the condition number of the matrix
 *
 * In general, the condition number of a matrix is the absolute value of the
 * largest singular value divided by the smallest singular value. When a matrix
 * is symmetric positive semi-definite, all eigenvalues are also singular
 * values. Moreover, all eigenvalues are non-negative.
 */
template <class MatrixType>
inline
double
SymmetricPositiveDefiniteEigenDecomposition<MatrixType>::conditionNo()
    const {

    const RealVectorType& ev = eigenvalues();

    double numerator = ev(ev.size() - 1);
    double denominator = ev(0);

    // All eigenvalues of a positive semi-definite matrix are
    // non-negative, so in theory no need to take absolute values.
    // Unfortunately, numerical instabilities can cause eigenvalues to
    // be slightly negative. We should interprete that as 0.
    if (denominator < 0)
        denominator = 0;

    return numerator <= 0 ? std::numeric_limits<double>::infinity()
                          : numerator / denominator;
}

/**
 * @brief Return the pseudo inverse previously computed using computeExtras().
 *
 * The result of this function is undefined if computeExtras() has not been
 * called or the pseudo-inverse was not set to be computed.
 */
template <class MatrixType>
inline
const MatrixType&
SymmetricPositiveDefiniteEigenDecomposition<MatrixType>::pseudoInverse() const {
    return mPinv;
}

/**
 * @brief Perform extra computations after the decomposition
 *
 * If the matrix has a condition number of less than 1e20 (currently
 * this is hard-coded), it necessarily has full rank and is invertible.
 * The Moore-Penrose pseudo-inverse coincides with the inverse and we
 * compute it directly, using a \f$ L D L^T \f$ Cholesky decomposition.
 *
 * If the matrix has a condition number of more than 1e20, we are on the
 * safe side and use the eigen decomposition for computing the
 * pseudo-inverse.
 *
 * Since the eigenvectors of a symmtric positive semi-definite matrix
 * are orthogonal, and Eigen moreover scales them to have norm 1 (i.e.,
 * the eigenvectors returned by Eigen are orthonormal), the Eigen
 * decomposition
 *
 *     \f$ M = V * D * V^T \f$
 *
 * is also a singular value decomposition (where M is the
 * original symmetric positive semi-definite matrix, D is the
 * diagonal matrix with eigenvectors, and V is the unitary
 * matrix containing normalized eigenvectors). In particular,
 * V is unitary, so the inverse can be computed as
 *
 *     \f$ M^{-1} = V * D^{-1} * V^T \f$.
 *
 * Only the <b>lower triangular part</b> of the input matrix
 * is referenced.
 */
template <class MatrixType>
inline
void
SymmetricPositiveDefiniteEigenDecomposition<MatrixType>::computeExtras(
    const MatrixType &inMatrix, int inExtras) {

    if (inExtras & ComputePseudoInverse) {
        mPinv.resize(inMatrix.rows(), inMatrix.cols());

        // FIXME: No hard-coded constant here
        if (conditionNo() < 1e20) {
            // We are doing a Cholesky decomposition of a matrix with
            // pivoting. This is faster than the PartialPivLU that
            // Eigen's inverse() method would use
            mPinv = inMatrix.template selfadjointView<Eigen::Lower>().ldlt()
                .solve(MatrixType::Identity(inMatrix.rows(), inMatrix.cols()));
        } else {
            if (!Base::m_eigenvectorsOk)
                Base::compute(inMatrix, Eigen::ComputeEigenvectors);

            const RealVectorType& ev = eigenvalues();

            // The eigenvalue are sorted in increasing order
            Scalar epsilon = static_cast<double>(inMatrix.rows())
                           * ev(ev.size() - 1)
                           * std::numeric_limits<Scalar>::epsilon();

            RealVectorType eigenvectorsInverted(ev.size());
            for (Index i = 0; i < static_cast<Index>(ev.size()); ++i) {
                eigenvectorsInverted(i) = ev(i) < epsilon
                                        ? Scalar(0)
                                        : Scalar(1) / ev(i);
            }
            mPinv = Base::eigenvectors()
                  * eigenvectorsInverted.asDiagonal()
                  * Base::eigenvectors().transpose();
        }
    }
}

} // namespace eigen_integration

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_DBAL_EIGEN_INTEGRATION_SPDEIGENDECOMPOSITION_IMPL_HPP)
