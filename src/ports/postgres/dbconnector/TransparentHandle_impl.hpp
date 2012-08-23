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

template <typename T, bool IsMutable>
TransparentHandle<T, IsMutable>::TransparentHandle(val_type* inPtr)
  : mPtr(inPtr) { }

/**
 * @brief Return the (constant) pointer of this handle
 */
template <typename T, bool IsMutable>
inline
typename TransparentHandle<T, IsMutable>::val_type*
TransparentHandle<T, IsMutable>::ptr() const {
    return mPtr;
}

// TransparentHandle<T, dbal::Mutable>

template <typename T>
TransparentHandle<T, dbal::Mutable>::TransparentHandle(val_type* inPtr)
  : Base(inPtr) { }

/**
 * @brief Return the pointer of this handle
 */
template <typename T>
inline
typename TransparentHandle<T, dbal::Mutable>::val_type*
TransparentHandle<T, dbal::Mutable>::ptr() {
    return const_cast<val_type*>(static_cast<Base*>(this)->ptr());
}

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_TRANSPARENTHANDLE_IMPL_HPP)
