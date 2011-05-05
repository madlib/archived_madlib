/* ----------------------------------------------------------------------- *//**
 *
 * @file Vector_const.hpp
 *
 * @brief MADlib immutable vector class -- a thin wrapper around arma::Col or arma::Row
 *
 * @internal Unfortunately, we have some code duplication from Vector.hpp. Also
 *      we need to define a proxy for all const operators and methods of
 *      arma::Col and arma::Row. This includes, e.g., operator(), operator[] and
 *      a few others.
 *
 *//* ----------------------------------------------------------------------- */

template<template <class> class T, typename eT>
class Vector_const : public arma::Base< eT, Vector_const<T, eT> > {
    friend class arma::unwrap< Vector_const<T,eT> >;
    friend class arma::Proxy< Vector_const<T,eT> >;

public:
    typedef eT elem_type;

    inline Vector_const(
        AllocatorSPtr inAllocator,
        const uint32_t inNumElem)
        : mMemoryHandle(
            inAllocator->allocateArray(
                inNumElem,
                static_cast<eT*>(NULL) /* pure type parameter */)),
          mVector(
            mMemoryHandle->ptr(),
            inNumElem,
            false /* copy_aux_mem */,
            true /* strict */),
          n_rows(mVector.n_rows),
          n_cols(mVector.n_cols),
          n_elem(mVector.n_elem)
        { }

    inline Vector_const(
        const MemHandleSPtr inHandle,
        const uint32_t inNumElem)
        : mMemoryHandle(inHandle),
          mVector(
            static_cast<eT*>(inHandle->ptr()),
            inNumElem,
            false /* copy_aux_mem */,
            true /* strict */),
          n_rows(mVector.n_rows),
          n_cols(mVector.n_cols),
          n_elem(mVector.n_elem)
        { }

    inline Vector_const(
        const Vector<T, eT> &inVec)
        : mMemoryHandle(inVec.mMemoryHandle),
          mVector(
            const_cast<eT*>(inVec.memptr()),
            inVec.n_elem,
            false /* copy_aux_mem */,
            true /* strict */),
          n_rows(mVector.n_rows),
          n_cols(mVector.n_cols),
          n_elem(mVector.n_elem)
        { }
    
    inline Vector_const(
        const Array<eT> &inArray)
        : mMemoryHandle(inArray.memoryHandle()),
          mVector(
            const_cast<eT*>(inArray.data()),
            inArray.size(),
            false /* copy_aux_mem */,
            true /* strict */),
          n_rows(mVector.n_rows),
          n_cols(mVector.n_cols),
          n_elem(mVector.n_elem)
        { }
    
   
    /**
     * @internal
     * This constructor will only be generated if \c T is a subclass of
     * \c const_multi_array_ref. This applies to Array<eT> and Array_const<eT>.
     */
    template <typename ArrayType>
    inline Vector_const(const ArrayType &inArray,
        typename enable_if<
                boost::is_base_of< const_multi_array_ref<eT, 1>, ArrayType >
            >::type *dummy = NULL)
        : mMemoryHandle(inArray.memoryHandle()),
          mVector(
            const_cast<eT*>(inArray.data()),
            inArray.size(),
            false /* copy_aux_mem */,
            true /* strict */),
          n_rows(mVector.n_rows),
          n_cols(mVector.n_cols),
          n_elem(mVector.n_elem)
        { }
    
    inline operator const T<eT>&() const {
        return mVector;
    }
        
    /**
     * @internal This function accesses internal elements of arma::mat.
     *      While these elements are all declared as public, this is not
     *      entirely clean.
     */
    inline Vector_const &rebind(const MemHandleSPtr inHandle,
        const uint32_t inNumElem) {
        
        using arma::access;
        using arma::Mat;
    
        // Unfortunately, C++ does not allow to partially specialize a member
        // function. (We would need to partially specialize the whole class
        // template.) For that reason, we use boost::is_same to do the check
        // at compile time instead of execution time.
        if (boost::is_same<T<eT>, arma::Col<eT> >::value)
            access::rw(mVector.n_rows) = inNumElem;
        else
            access::rw(mVector.n_cols) = inNumElem;
        
        access::rw(mVector.n_elem) = inNumElem;
        access::rw(mVector.mem) = static_cast<eT*>(inHandle->ptr());
        mMemoryHandle = inHandle;
        return *this;
    }

    inline MemHandleSPtr memoryHandle() const {
        return mMemoryHandle;
    }
    
protected:
    MemHandleSPtr mMemoryHandle;
    const T<eT> mVector;

public:
    const arma::u32 &n_rows;    //!< number of rows in the matrix (read-only)
    const arma::u32 &n_cols;    //!< number of columns in the matrix (read-only)
    const arma::u32 &n_elem;
};
