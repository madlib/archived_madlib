template <typename T>
inline
const T*
AbstractionLayer::TransparentHandle<T>::ptr() const {
    return mPtr;
}

template <typename T>
inline
T*
AbstractionLayer::MutableTransparentHandle<T>::ptr() {
    return mPtr;
}
