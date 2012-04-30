/* ----------------------------------------------------------------------- *//**
 *
 * @file TransparentHandle_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_TRANSPARENTHANDLE_IMPL_HPP
#define MADLIB_POSTGRES_TRANSPARENTHANDLE_IMPL_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

/**
 * @brief Return the (constant) pointer of this handle
 */
template <typename T>
inline
const T*
TransparentHandle<T>::ptr() const {
    return mPtr;
}

/**
 * @brief Return the pointer of this handle
 */
template <typename T>
inline
T*
MutableTransparentHandle<T>::ptr() {
    return mPtr;
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_TRANSPARENTHANDLE_IMPL_HPP)
