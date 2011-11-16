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

    template <class EigenType>
    class HandleMap
      : public Eigen::Map<EigenType, MapOptions> {
    public:
        typedef Eigen::Map<EigenType, MapOptions> Base;
        typedef typename Base::Scalar Scalar;
        typedef typename Base::Index Index;

        inline HandleMap()
          : Base(NULL, 1, 1) { }

        inline HandleMap(
            AllocatorSPtr inAllocator,
            const uint32_t inNumElem)
          : Base(NULL, 1, 1),
            mMemoryHandle(
                inAllocator->allocateArray(
                    inNumElem,
                    static_cast<Scalar*>(NULL) /* pure type parameter */)) {
            
            new (this) HandleMap(mMemoryHandle, inNumElem);
        }
        
        inline HandleMap(
            MemHandleSPtr inHandle,
            Index inNumElem)
          : Base(static_cast<Scalar*>(inHandle->ptr()), inNumElem),
            mMemoryHandle(inHandle) { }
        
        inline HandleMap(
            MemHandleSPtr inHandle,
            Index inNumRows,
            Index inNumCols)
          : Base(static_cast<Scalar*>(inHandle->ptr()), inNumRows, inNumCols),
            mMemoryHandle(inHandle) { }
        
        inline HandleMap(
            const HandleMap& inMat)
          : Base(NULL, 1, 1),
            mMemoryHandle(
                AbstractHandle::cloneIfNotGlobal(inMat.mMemoryHandle)) {
            
            new (this) HandleMap(mMemoryHandle, inMat.rows(), inMat.cols());
        }
        
        inline HandleMap(
            const Array<Scalar> &inArray)
          : Base(NULL, 1, 1),
            mMemoryHandle(
                AbstractHandle::cloneIfNotGlobal(inArray.memoryHandle())) {
            
            new (this) HandleMap(mMemoryHandle, inArray.size());
        }

        inline HandleMap(
            const Array_const<Scalar> &inArray)
          : Base(NULL, 1, 1),
            mMemoryHandle(
                AbstractHandle::cloneIfNotGlobal(inArray.memoryHandle())) {
            
            // FIXME: A compile-time error would be nicer!
            if (!boost::is_const<EigenType>::value)
                throw std::runtime_error("Internal error: Cannot initialize "
                    "mutable vector with immutable array.");
            
            new (this) HandleMap(mMemoryHandle, inArray.size());
        }
        
        inline HandleMap& operator=(const HandleMap& other) {
            this->Base::operator=(other);
            return *this;
        }
        
        inline HandleMap& rebind(const MemHandleSPtr inHandle,
            const Index inSize) {
            
            new (this) HandleMap(inHandle, inSize);
            mMemoryHandle = inHandle;
            return *this;
        }
        
        inline HandleMap& rebind(const MemHandleSPtr inHandle,
            const Index inRows, const Index inCols) {
            
            new (this) HandleMap(inHandle, inRows, inCols);
            mMemoryHandle = inHandle;
            return *this;
        }

        inline MemHandleSPtr memoryHandle() const {
            return mMemoryHandle;
        }

    protected:
        MemHandleSPtr mMemoryHandle;
    };

    typedef HandleMap<Eigen::MatrixXd> DoubleMat;
    typedef HandleMap<const Eigen::MatrixXd> DoubleMat_const;
    typedef HandleMap<Eigen::VectorXd> DoubleCol;
    typedef HandleMap<const Eigen::VectorXd> DoubleCol_const;
    typedef HandleMap<Eigen::RowVectorXd> DoubleRow;
    typedef HandleMap<const Eigen::RowVectorXd> DoubleRow_const;
 
    template <typename Derived>
    inline
    typename Eigen::MatrixBase<Derived>::ConstTransposeReturnType
    static trans(const Eigen::MatrixBase<Derived>& mat) {
        return mat.transpose();
    }
};
