template <typename T>
inline
AbstractionLayer::ArrayHandle<T>::ArrayHandle(const ArrayType *inArray)
  : mArray(inArray) { }

template <typename T>
inline 
const T*
AbstractionLayer::ArrayHandle<T>::ptr() const {
    return reinterpret_cast<T*>(ARR_DATA_PTR(mArray));
}

template <typename T>
inline 
size_t
AbstractionLayer::ArrayHandle<T>::size() const {
    return internalArraySize(mArray);
}

template <typename T>
inline 
const ArrayType*
AbstractionLayer::ArrayHandle<T>::array() const {
    return mArray;
}

template <typename T>
inline 
const T&
AbstractionLayer::ArrayHandle<T>::operator[](size_t inIndex) const {
    return ptr()[inIndex];
}

template <typename T>
inline
size_t
AbstractionLayer::ArrayHandle<T>::internalArraySize(const ArrayType *inArray) {
    // FIXME: Add support for multi-dimensional array
    return ARR_DIMS(inArray)[0];
}

/**
 * @brief Constructor with immutable native PosgreSQL array
 *
 * Performs a copy of the PostgreSQL array.
 */
template <typename T>
inline 
AbstractionLayer::MutableArrayHandle<T>::MutableArrayHandle(
    const ArrayType *inArray)
  : Base(static_cast<ArrayType*>(
        defaultAllocator().allocate<
            dbal::FunctionContext,
            dbal::DoNotZero,
            dbal::ThrowBadAlloc>(VARSIZE(inArray))
    )) {
    
    std::memcpy(array(), inArray, VARSIZE(inArray));
}


template <typename T>
inline 
T*
AbstractionLayer::MutableArrayHandle<T>::ptr() {
    return const_cast<T*>(static_cast<const ArrayHandle<T>*>(this)->ptr());
}

template <typename T>
inline 
ArrayType*
AbstractionLayer::MutableArrayHandle<T>::array() {
    return const_cast< ArrayType* >(Base::mArray);
}

template <typename T>
inline 
T&
AbstractionLayer::MutableArrayHandle<T>::operator[](size_t inIndex) {
    return const_cast<T&>(
        static_cast<const ArrayHandle<T>*>(this)->operator[](inIndex)
    );
}
