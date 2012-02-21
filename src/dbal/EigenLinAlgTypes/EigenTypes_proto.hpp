/* ----------------------------------------------------------------------- *//**
 *
 * @file EigenTypes_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by EigenLinAlgTypes.hpp
#ifdef MADLIB_EIGEN_LINALGTYPES_HPP

/**
 * @brief Wrapper class for linear-algebra types based on Eigen
 */
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
    class HandleMap;

    typedef Eigen::VectorXd ColumnVector;
    typedef Eigen::Vector2d ColumnVector2;
    typedef Eigen::Vector3d ColumnVector3;
    typedef Eigen::RowVectorXd RowVector;
    typedef Eigen::MatrixXd Matrix;
    typedef EIGEN_DEFAULT_DENSE_INDEX_TYPE Index;
    
    enum ViewMode {
        Lower = Eigen::Lower,
        Upper = Eigen::Upper
    };
    
    enum DecompositionOptions {
        ComputeEigenvectors = Eigen::ComputeEigenvectors,
        EigenvaluesOnly = Eigen::EigenvaluesOnly
    };
    
    enum SPDDecompositionExtras {
        ComputePseudoInverse = 0x01
    };
    
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
    
    template <ViewMode Mode, typename Derived>
    inline
    typename Eigen::MatrixBase<Derived>::template TriangularViewReturnType<Mode>::Type
    static triangularView(Eigen::MatrixBase<Derived>& mat) {
        return mat.triangularView<Mode>();
    }
    
    template <typename Derived>
    bool
    static isfinite(const Eigen::MatrixBase<Derived>& mat) {
        return mat.is_finite();
    }
    
    template <class MatrixType>
    class SymmetricPositiveDefiniteEigenDecomposition;
};

#endif // MADLIB_EIGEN_LINALGTYPES_HPP (workaround for Doxygen)
