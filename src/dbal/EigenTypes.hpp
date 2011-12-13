/* ----------------------------------------------------------------------- *//**
 *
 * @file EigenIntegration.hpp
 *
 * @brief Helper classes for integrating Eigen
 *
 * @internal
 *     Using the placement new syntax is endorsed by the Eigen developers
 *     http://eigen.tuxfamily.org/dox-devel/classEigen_1_1Map.html
 *
 *//* ----------------------------------------------------------------------- */

template <int MapOptions>
struct EigenTypes {
protected:
    template <
        class EigenType,
        bool EigenTypeIsConst = boost::is_const<EigenType>::value
    > struct DefaultHandle;
    
    template <class EigenType>
    struct DefaultHandle<EigenType, true> {
        typedef AbstractionLayer::ArrayHandle<typename EigenType::Scalar> type;
    };
    template <class EigenType>
    struct DefaultHandle<EigenType, false> {
        typedef AbstractionLayer::MutableArrayHandle<typename EigenType::Scalar> type;
    };
    
public:
    template <class EigenType, class Handle = typename DefaultHandle<EigenType>::type >
    class HandleMap
      : public Eigen::Map<EigenType, MapOptions> {
    public:
        typedef Eigen::Map<EigenType, MapOptions> Base;
        typedef typename Base::Scalar Scalar;
        typedef typename Base::Index Index;
        
        using Base::operator=;

        inline HandleMap()
          : Base(NULL, 1, 1), mMemoryHandle(NULL) { }

        inline HandleMap(
            const Handle &inHandle)
          : Base(const_cast<Scalar*>(inHandle.ptr()), inHandle.size()),
            mMemoryHandle(inHandle) {
        }

        inline HandleMap(
            const Handle &inHandle,
            Index inNumElem)
          : Base(const_cast<Scalar*>(inHandle.ptr()), inNumElem),
            mMemoryHandle(inHandle) { }
        
        /**
         * @internal
         *     We need to do a const cast here: There is no alternative to
         *     initializing from "const Handle&", but the base class constructor
         *     needs a non-const value. We therefore make sure that a
         *     non-mutable handle can only be assigned if EigenType is a
         *     constant type. See the static assert below.
         */
        inline HandleMap(
            const Handle &inHandle,
            Index inNumRows,
            Index inNumCols)
          : Base(const_cast<Scalar*>(inHandle.ptr()), inNumRows, inNumCols),
            mMemoryHandle(inHandle) { }
        
        operator AbstractionLayer::AnyType() const {
            return mMemoryHandle;
        }
        
        inline HandleMap& operator=(const HandleMap& other) {
            Base::operator=(other);
            return *this;
        }
        
        inline HandleMap& rebind(const Handle &inHandle) {
            return rebind(inHandle, inHandle.size());
        }
        
        inline HandleMap& rebind(const Handle &inHandle,
            const Index inSize) {
            
            new (this) HandleMap(inHandle, inSize);
            mMemoryHandle = inHandle;
            return *this;
        }
        
        inline HandleMap& rebind(const Handle &inHandle,
            const Index inRows, const Index inCols) {
            
            new (this) HandleMap(inHandle, inRows, inCols);
            mMemoryHandle = inHandle;
            return *this;
        }

        inline const Handle &memoryHandle() const {
            return mMemoryHandle;
        }

    protected:
        Handle mMemoryHandle;
        
    private:
        BOOST_STATIC_ASSERT_MSG(
            Handle::isMutable || boost::is_const<EigenType>::value,
            "non-const matrix cannot be backed by immutable handle");
    };

    typedef Eigen::VectorXd ColumnVector;
    typedef Eigen::RowVectorXd RowVector;
    typedef Eigen::MatrixXd Matrix;
     
    template <typename Derived>
    inline
    typename Eigen::MatrixBase<Derived>::ConstTransposeReturnType
    static trans(const Eigen::MatrixBase<Derived>& mat) {
        return mat.transpose();
    }
    
    template <typename Derived, typename OtherDerived>
    inline
    typename Eigen::internal::scalar_product_traits<
        typename Eigen::internal::traits<Derived>::Scalar,
        typename Eigen::internal::traits<OtherDerived>::Scalar
    >::ReturnType
    static dot(
        const Eigen::MatrixBase<Derived>& mat,
        const Eigen::MatrixBase<OtherDerived>& other) {
        return mat.dot(other);
    }
    
    template <typename Derived>
    inline
    typename Eigen::MatrixBase<Derived>::CoeffReturnType
    static as_scalar(const Eigen::MatrixBase<Derived>& mat) {
        return mat.value();
    }
    
    template <typename Derived>
    bool
    static isfinite(const Eigen::MatrixBase<Derived>& mat) {
        return mat.is_finite();
    }
    
    enum DecompositionOptions {
        ComputeEigenvectors = Eigen::ComputeEigenvectors,
        EigenvaluesOnly = Eigen::EigenvaluesOnly
    };
    
    enum SPDDecompositionExtras {
        ComputePseudoInverse = 0x01
    };

    /**
     * @brief Computes eigenvalues, eigenvectors, and pseudo-inverse of
     *     symmetric positive semi-definite matrices
     *
     * A matrix is symmetric if it equals its transpose. It is positive
     * semi-definite if all its eigenvalues are non-negative. This class
     * computes the eigenvalues, the eigenvectors, and the Moore-Penrose
     * pseudo-inverse of a symmetric positive semi-definite matrix.
     */
    template <class MatrixType>
    class SymmetricPositiveDefiniteEigenDecomposition
      : public Eigen::SelfAdjointEigenSolver<MatrixType> {
      
        typedef Eigen::SelfAdjointEigenSolver<MatrixType> Base;
        typedef typename Base::Scalar Scalar;

    public:    
        typedef typename Base::RealVectorType RealVectorType;
    
        using Base::eigenvalues;

        SymmetricPositiveDefiniteEigenDecomposition(const MatrixType &inMatrix,
            int inOptions = ComputeEigenvectors, int inExtras = 0)
          : Base(inMatrix, inOptions) {
            
            computeExtras(inMatrix, inExtras);
        }
        
        double conditionNo() const {
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
        
        const MatrixType &pseudoInverse() const {
            return mPinv;
        }
        
    protected:
        void computeExtras(const MatrixType &inMatrix, int inExtras) {
            if (inExtras & ComputePseudoInverse) {
                /**
                 * If the matrix is well-conditioned, we just call the inverse
                 * function. Otherwise, we use the eigen decomposition:
                 * Since the eigenvectors returned by Eigen form an orthogonal
                 * matrix (the columns/rows are orthonormal), the decomposition
                 *
                 *     \f$ M = V * D * V^T \f$
                 *
                 * is also a singular value decomposition (where M is the
                 * original symmetric positive semi-definite matrix, D is the
                 * diagonal matrix with eigenvectors, and V is the matrix
                 * containing normalized eigenvectors).
                 *
                 * Only the <b>lower triangular part</b> of the input matrix
                 * is referenced.
                 */
                mPinv.resize(inMatrix.rows(), inMatrix.cols());

                // FIXME: No hard-coded constant here
                if (conditionNo() < 1000) {
                    // We are doing a Cholesky decomposition of a matrix with
                    // pivoting. This is faster than the PartialPivLU that
                    // Eigen's inverse() method would use
                    mPinv = inMatrix.template selfadjointView<Eigen::Lower>().ldlt().solve(
                        MatrixType::Identity(inMatrix.rows(), inMatrix.cols()));
                } else {
                    if (!Base::m_eigenvectorsOk)
                        Base::compute(inMatrix, Eigen::ComputeEigenvectors);
                    
                    const RealVectorType& ev = eigenvalues();
                    
                    // The eigenvalue are sorted in increasing order
                    Scalar epsilon = inMatrix.rows()
                                   * ev(ev.size() - 1)
                                   * std::numeric_limits<Scalar>::epsilon();
                    
                    RealVectorType eigenvectorsInverted(ev.size());
                    for (Index i = 0; i < ev.size(); i++) {
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

        MatrixType mPinv;
    };
};
