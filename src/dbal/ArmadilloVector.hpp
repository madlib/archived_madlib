/* ----------------------------------------------------------------------- *//**
 *
 * @file Vector.hpp
 *
 * @brief MADlib mutable vector class, a thin wrapper around arma::Col or arma::Row
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief MADlib mutable matrix class -- a thin wrapper around arma::Col or
 *        arma::Row
 *
 * Armadillo does not provide a public interface to rebind the chunk of memory 
 * an arma::Mat<eT> object is using. We therefore need this subclass to 
 * make matrix objects first-class citizen in the C++ DBAL.
 *
 * @internal Inheritance is not without issues here, and in a future version
 *     we might want to switch to composition instead of inheritance (in order
 *     to make it less likely that changes in the superclass break our
 *     implementation).
 */
template<template <class> class T, typename eT>
class Vector : public T<eT> {
public:
    inline Vector()
        : T<eT>(
            NULL,
            0,
            false /* copy_aux_mem */,
            true /* strict */),
          mMemoryHandle() { }

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
        
        arma::access::rw(arma::Mat<eT>::mem) =
            static_cast<eT*>(mMemoryHandle->ptr());
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
            NULL,
            inVec.n_elem,
            false /* copy_aux_mem */,
            true /* strict */),
          mMemoryHandle(
            AbstractHandle::cloneIfNotGlobal(inVec.mMemoryHandle)) {
        
        arma::access::rw(arma::Mat<eT>::mem) =
            static_cast<eT*>(mMemoryHandle->ptr());
    }
    
    inline Vector(
        const Array<eT> &inArray)
        : T<eT>(
            NULL,
            inArray.size(),
            false /* copy_aux_mem */,
            true /* strict */),
          mMemoryHandle(
            AbstractHandle::cloneIfNotGlobal(inArray.memoryHandle())) {
        
        arma::access::rw(arma::Mat<eT>::mem) =
            static_cast<eT*>(mMemoryHandle->ptr());
    }
    
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

    inline Vector &noalias() {
        // Do nothing. This function is only for API compatibility with Eigen.
        return *this;
    }
    
    inline uint64_t size() const {
        return arma::Mat<eT>::n_elem;
    }

protected:
    MemHandleSPtr mMemoryHandle;
};
