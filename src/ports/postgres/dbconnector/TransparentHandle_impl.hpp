/* ----------------------------------------------------------------------- *//**
 *
 * @file TransparentHandle_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Return the (constant) pointer of this handle
 */
template <typename T>
inline
const T*
AbstractionLayer::TransparentHandle<T>::ptr() const {
    return mPtr;
}

/**
 * @brief Return the pointer of this handle
 */
template <typename T>
inline
T*
AbstractionLayer::MutableTransparentHandle<T>::ptr() {
    return mPtr;
}
