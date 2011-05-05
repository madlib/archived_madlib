/* ----------------------------------------------------------------------- *//**
 *
 * @file Vector.hpp
 *
 * @brief MADlib mutable vector class, a thin wrapper around arma::Col or arma::Row
 *
 *//* ----------------------------------------------------------------------- */

template<template <class> class T, typename eT>
class Vector : public T<eT> {
public:
    inline Vector(
        AllocatorSPtr inAllocator,
        const uint32_t inNumElem)
        : T<eT>(
            NULL,
            inNumElem,
            false /* copy_aux_mem */,
            true /* strict */),
          mMemoryHandle(
            inAllocator->allocateArray(
                inNumElem,
                static_cast<eT*>(NULL) /* pure type parameter */)) {
        
        using arma::access;
        using arma::Mat;
        
        access::rw(Mat<eT>::mem) = static_cast<eT*>(mMemoryHandle->ptr());
    }

    inline Vector(
        const MemHandleSPtr inHandle,
        const uint32_t inNumElem)
        : T<eT>(
            static_cast<eT*>(inHandle->ptr()),
            inNumElem,
            false /* copy_aux_mem */,
            true /* strict */),
          mMemoryHandle(inHandle)
        { }

    inline Vector(
        const Vector<T, eT> &inVec)
        : T<eT>(
            const_cast<eT*>(inVec.memptr()),
            inVec.n_elem,
            false /* copy_aux_mem */,
            true /* strict */),
          mMemoryHandle(inVec.mMemoryHandle)
        { }
    
    inline Vector(
        const Array<eT> &inArray)
        : T<eT>(
            const_cast<eT*>(inArray.data()),
            inArray.size(),
            false /* copy_aux_mem */,
            true /* strict */),
          mMemoryHandle(inArray.memoryHandle())
        { }
    
    template<typename T1>
    inline const Vector &operator=(const arma::Base<eT,T1>& X) {
        T<eT>::operator=(X.get_ref());
        return *this;
    }
    
    inline Vector &rebind(const MemHandleSPtr inHandle, const uint32_t inNumElem) {
        using arma::access;
        using arma::Mat;
    
        // Unfortunately, C++ does not allow to partially specialize a member
        // function. (We would need to partially specialize the whole class
        // template.) For that reason, we use boost::is_same to do the check
        // at compile time instead of execution time.
        if (boost::is_same<T<eT>, arma::Col<eT> >::value)
            access::rw(Mat<eT>::n_rows) = inNumElem;
        else
            access::rw(Mat<eT>::n_cols) = inNumElem;
        
        access::rw(Mat<eT>::n_elem) = inNumElem;
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
