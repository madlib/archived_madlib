/* ----------------------------------------------------------------------- *//**
 *
 * @file Matrix.hpp
 *
 * @brief The MADlib Matrix class -- a thin wrapper around arma::Mat
 *
 *//* ----------------------------------------------------------------------- */

template<typename eT>
class Matrix : public arma::Mat<eT> {
public:
    inline Matrix(
        AllocatorSPtr inAllocator,
        const uint32_t inNumRows,
        const uint32_t inNumCols)
        : arma::Mat<eT>(
            NULL,
            inNumRows,
            inNumCols,
            false /* copy_aux_mem */,
            true /* strict */),
          mMemoryHandle(
            inAllocator->allocateArray(
                inNumRows * inNumCols,
                static_cast<eT*>(NULL) /* pure type parameter */)) {
        
        using arma::access;
        using arma::Mat;
        
        access::rw(Mat<eT>::mem) = mMemoryHandle->ptr();
    }

    inline Matrix(
        const MemHandleSPtr inHandle,
        const uint32_t inNumRows,
        const uint32_t inNumCols)
        : arma::Mat<eT>(
            static_cast<eT*>(inHandle->ptr()),
            inNumRows,
            inNumCols,
            false /* copy_aux_mem */,
            true /* strict */),
          mMemoryHandle(inHandle)
        { }
    
    inline Matrix(
        const Matrix<eT> &inMat)
        : arma::Mat<eT>(
            const_cast<eT*>(inMat.memptr()),
            inMat.n_rows,
            inMat.n_cols,
            false /* copy_aux_mem */,
            true /* strict */),
          mMemoryHandle(inMat.mMemoryHandle)
        { }
    
    template<typename T1>
    inline const Matrix &operator=(const arma::Base<eT,T1>& X) {
        using arma::Mat;
    
        Mat<eT>::operator=(X.get_ref());
        return *this;
    }

    inline Matrix &rebind(
        const MemHandleSPtr inHandle,
        const uint32_t inNumRows,
        const uint32_t inNumCols) {
        
        using arma::access;
        using arma::Mat;
        
        access::rw(Mat<eT>::n_rows) = inNumRows;
        access::rw(Mat<eT>::n_cols) = inNumCols;
        access::rw(Mat<eT>::n_elem) = inNumRows * inNumCols;
        access::rw(Mat<eT>::mem) = static_cast<eT*>(inHandle->ptr());
        mMemoryHandle = inHandle;
        return *this;
    }

    inline MemHandleSPtr memoryHandle() const {
        return mMemoryHandle;
    }

protected:
    MemHandleSPtr mMemoryHandle;
};
