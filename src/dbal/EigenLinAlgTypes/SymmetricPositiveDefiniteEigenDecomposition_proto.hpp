/* ----------------------------------------------------------------------- *//**
 *
 * @file SymmetricPositiveDefiniteEigenDecomposition_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by EigenLinAlgTypes.hpp
#ifdef MADLIB_EIGEN_LINALGTYPES_HPP

/**
 * @brief Computes eigenvalues, eigenvectors, and pseudo-inverse of
 *     symmetric positive semi-definite matrices
 *
 * A matrix is symmetric if it equals its transpose. It is positive
 * semi-definite if all its eigenvalues are non-negative. This class
 * computes the eigenvalues, the eigenvectors, and the Moore-Penrose
 * pseudo-inverse of a symmetric positive semi-definite matrix.
 */
template <int MapOptions>
template <class MatrixType>
class EigenTypes<MapOptions>::SymmetricPositiveDefiniteEigenDecomposition
  : public Eigen::SelfAdjointEigenSolver<MatrixType> {
  
    typedef Eigen::SelfAdjointEigenSolver<MatrixType> Base;
    typedef typename Base::Scalar Scalar;

public:    
    typedef typename Base::RealVectorType RealVectorType;

    using Base::eigenvalues;

    SymmetricPositiveDefiniteEigenDecomposition(const MatrixType &inMatrix,
        int inOptions = ComputeEigenvectors, int inExtras = 0);
    
    double conditionNo() const;
    
    const MatrixType &pseudoInverse() const;
    
protected:
    void computeExtras(const MatrixType &inMatrix, int inExtras);
    
    MatrixType mPinv;
};

#endif // MADLIB_EIGEN_LINALGTYPES_HPP (workaround for Doxygen)
