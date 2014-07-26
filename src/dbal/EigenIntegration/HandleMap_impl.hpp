/* ----------------------------------------------------------------------- *//**
 *
 * @file HandleMap_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_EIGEN_HANDLEMAP_IMPL_HPP
#define MADLIB_DBAL_EIGEN_HANDLEMAP_IMPL_HPP

namespace madlib {

namespace dbal {

namespace eigen_integration {

/**
 * @brief Assignment operator
 *
 * Handle in the same way as assignments of any other matrix object.
 */
template <class EigenType, class Handle, int MapOptions>
inline
HandleMap<EigenType, Handle, MapOptions>&
HandleMap<EigenType, Handle, MapOptions>::operator=(
    const HandleMap& other) {

    madlib_assert(this->rows() == other.rows() &&
                  this->cols() == other.cols(),
        std::runtime_error("HandleMap::operator= found unmatched dimensions. "
                           "To change dimensions, use rebind()"));

    Base::operator=(other);
    return *this;
}

/**
 * @brief Rebind to a different handle
 *
 * This ignores any size information the handle may have. The format of the
 * matrix is not changed.
 */
template <class EigenType, class Handle, int MapOptions>
inline
HandleMap<EigenType, Handle, MapOptions>&
HandleMap<EigenType, Handle, MapOptions>::rebind(
    const Handle &inHandle) {

    return rebind(inHandle, this->rows(), this->cols());
}

/**
 * @brief Rebind to a different handle
 *
 * This ignores any size information the handle may have. This
 * rebind method can be used with any Handle.
 */
template <class EigenType, class Handle, int MapOptions>
inline
HandleMap<EigenType, Handle, MapOptions>&
HandleMap<EigenType, Handle, MapOptions>::rebind(
    const Handle &inHandle, const Index inSize) {

    new (this) HandleMap(inHandle, inSize);
    mMemoryHandle = inHandle;
    return *this;
}

template <class EigenType, class Handle, int MapOptions>
inline
HandleMap<EigenType, Handle, MapOptions>&
HandleMap<EigenType, Handle, MapOptions>::rebind(
    const Index inSize) {

    return this->rebind(mMemoryHandle, inSize);
}

/**
 * @brief Rebind to a different handle
 *
 * This ignores any size information the handle may have. This
 * rebind method can be used with any Handle.
 *
 * @internal
 *     Using the placement new syntax is endorsed by the Eigen developers
 *     http://eigen.tuxfamily.org/dox-devel/classEigen_1_1Map.html
 */
template <class EigenType, class Handle, int MapOptions>
inline
HandleMap<EigenType, Handle, MapOptions>&
HandleMap<EigenType, Handle, MapOptions>::rebind(
    const Handle &inHandle, const Index inRows, const Index inCols) {

    new (this) HandleMap(inHandle, inRows, inCols);
    mMemoryHandle = inHandle;
    return *this;
}

template <class EigenType, class Handle, int MapOptions>
inline
HandleMap<EigenType, Handle, MapOptions>&
HandleMap<EigenType, Handle, MapOptions>::rebind(
    const Index inRows, const Index inCols) {

    return this->rebind(mMemoryHandle, inRows, inCols);
}

/**
 * @brief Return the Handle that backs this HandleMap.
 */
template <class EigenType, class Handle, int MapOptions>
inline
const Handle&
HandleMap<EigenType, Handle, MapOptions>::memoryHandle() const {
    return mMemoryHandle;
}

} // namespace eigen_integration

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_DBAL_EIGEN_HANDLEMAP_IMPL_HPP)
