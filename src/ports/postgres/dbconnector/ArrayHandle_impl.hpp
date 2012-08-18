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
ArrayHandle<T>::ArrayHandle(const ArrayType *inArray)
  : mArray(inArray) { }

template <typename T>
inline 
const T*
ArrayHandle<T>::ptr() const {
    return reinterpret_cast<T*>(ARR_DATA_PTR(mArray));
}

template <typename T>
inline 
size_t
ArrayHandle<T>::size() const {
    // An empty array has dimensionality 0.
    size_t arraySize = ARR_NDIM(mArray) ? 1 : 0;
    for (int i = 0; i < ARR_NDIM(mArray); ++i)
        arraySize *= ARR_DIMS(mArray)[i];
    return arraySize;
}

template <typename T>
inline 
size_t
ArrayHandle<T>::dims() const {
    return ARR_NDIM(mArray);
}

template <typename T>
inline 
size_t
ArrayHandle<T>::sizeOfDim(size_t inDim) const {
    if (inDim >= dims())
        throw std::invalid_argument("Invalid dimension.");
    
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
