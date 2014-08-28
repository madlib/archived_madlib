/* ----------------------------------------------------------------------- *//**
 *
 * @file ArrayHandle_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_ARRAYHANDLE_IMPL_HPP
#define MADLIB_POSTGRES_ARRAYHANDLE_IMPL_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

template <typename T>
inline
ArrayHandle<T>::ArrayHandle(ArrayType *inArray): mArray(inArray) {
    if (inArray == NULL) {
        mData = NULL;
        mNumElems = -1;
        mElemLen = -1;
        mElemByVal = false;
        mElemAlign = -1;
    } else {
        madlib_get_typlenbyvalalign(ARR_ELEMTYPE(inArray),
                                    &mElemLen,
                                    &mElemByVal,
                                    &mElemAlign);
        if (mElemByVal) {
            mData = reinterpret_cast<T*>(ARR_DATA_PTR(inArray));
            mNumElems = static_cast<int>(this->size());
        } else {
            // FIXME deallocate memory for datum_ptr and mData!
            Datum *datum_ptr;
            deconstruct_array(inArray,
                              ARR_ELEMTYPE(inArray),
                              mElemLen,
                              mElemByVal,
                              mElemAlign,
                              &datum_ptr,
                              NULL,
                              &mNumElems);
            mData = new T[mNumElems];
            for (int i = 0; i < mNumElems; i ++) {
                varlena *detoast_ptr = PG_DETOAST_DATUM(datum_ptr[i]);
                char *index_ptr = reinterpret_cast<char*>(&detoast_ptr);
                mData[i] = *(reinterpret_cast<T*>(index_ptr));
            }
        }
    }
}

template <typename T>
inline
const T*
ArrayHandle<T>::ptr() const {
    if (mArray == NULL) { return NULL; }

    if (mElemByVal) {
        return reinterpret_cast<T*>(ARR_DATA_PTR(mArray));
    } else {
        return reinterpret_cast<T*>(mData);
    }
}

template <typename T>
inline
size_t
ArrayHandle<T>::size() const {
    madlib_assert(ptr() != NULL, std::runtime_error(
        "Attempt to getting size() of a NULL array detected."));

    // An empty array has dimensionality 0.
    size_t arraySize = ARR_NDIM(mArray) ? 1 : 0;
    for (int i = 0; i < ARR_NDIM(mArray); ++i) {
        arraySize *= ARR_DIMS(mArray)[i];
    }

    return arraySize;
}

template <typename T>
inline
size_t
ArrayHandle<T>::dims() const {
    madlib_assert(ptr() != NULL, std::runtime_error(
        "Attempt to getting dims() of a NULL array detected."));

    return ARR_NDIM(mArray);
}

template <typename T>
inline
size_t
ArrayHandle<T>::sizeOfDim(size_t inDim) const {
    if (inDim >= dims()) {
        throw std::invalid_argument("Invalid dimension.");
    }

    return ARR_DIMS(mArray)[inDim];
}

template <typename T>
inline
const ArrayType*
ArrayHandle<T>::array() const {
    return mArray;
}

template <typename T>
inline
const T&
ArrayHandle<T>::operator[](size_t inIndex) const {
    madlib_assert(ptr() != NULL, std::runtime_error(
        "Indexing (operator[]) into a NULL array detected."));
    madlib_assert(inIndex < size(), std::runtime_error(
        "Out-of-bounds array access detected."));

    return ptr()[inIndex];
}

template <typename T>
inline
T*
MutableArrayHandle<T>::ptr() {
    return const_cast<T*>(static_cast<const ArrayHandle<T>*>(this)->ptr());
}

template <typename T>
inline
ArrayType*
MutableArrayHandle<T>::array() {
    return const_cast< ArrayType* >(Base::mArray);
}

template <typename T>
inline
T&
MutableArrayHandle<T>::operator[](size_t inIndex) {
    return const_cast<T&>(
        static_cast<const ArrayHandle<T>*>(this)->operator[](inIndex)
    );
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_ARRAYHANDLE_IMPL_HPP)
