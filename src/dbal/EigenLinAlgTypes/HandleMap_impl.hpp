/* ----------------------------------------------------------------------- *//**
 *
 * @file HandleMap_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by EigenLinAlgTypes.hpp
#ifdef MADLIB_EIGEN_LINALGTYPES_HPP

/**
 * @brief Conversion operator for AnyType
 *
 * A matrix or vector is just an array to the backend. We use the usual
 * conversion operations for arrays.
 */
template <int MapOptions>
template <class EigenType, class Handle>
inline
EigenTypes<MapOptions>::HandleMap<EigenType, Handle>::
    operator AbstractionLayer::AnyType() const {

    return mMemoryHandle;
}

/**
 * @brief Assignment operator
 *
 * Handle in the same way as assignments of any other matrix object.
 */
template <int MapOptions>
template <class EigenType, class Handle>
inline
EigenTypes<MapOptions>::HandleMap<EigenType, Handle>&
EigenTypes<MapOptions>::HandleMap<EigenType, Handle>::operator=(
    const HandleMap& other) {

    Base::operator=(other);
    return *this;
}
        
/**
 * @brief Rebind to a different handle
 *
 * This rebind method requires that Handle has a size() method, which is
 * called to determine the length of the vector.
 */
template <int MapOptions>
template <class EigenType, class Handle>
inline
EigenTypes<MapOptions>::HandleMap<EigenType, Handle>&
EigenTypes<MapOptions>::HandleMap<EigenType, Handle>::rebind(
    const Handle &inHandle) {

    return rebind(inHandle, inHandle.size());
}

/**
 * @brief Rebind to a different handle
 * 
 * This ignores any size information the handle may have. This
 * rebind method can be used with any Handle.
 */
template <int MapOptions>
template <class EigenType, class Handle>
inline
EigenTypes<MapOptions>::HandleMap<EigenType, Handle>&
EigenTypes<MapOptions>::HandleMap<EigenType, Handle>::rebind(
    const Handle &inHandle, const Index inSize) {
    
    new (this) HandleMap(inHandle, inSize);
    mMemoryHandle = inHandle;
    return *this;
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
template <int MapOptions>
template <class EigenType, class Handle>
inline
EigenTypes<MapOptions>::HandleMap<EigenType, Handle>&
EigenTypes<MapOptions>::HandleMap<EigenType, Handle>::rebind(
    const Handle &inHandle, const Index inRows, const Index inCols) {
    
    new (this) HandleMap(inHandle, inRows, inCols);
    mMemoryHandle = inHandle;
    return *this;
}

/**
 * @brief Return the Handle that backs this HandleMap.
 */
template <int MapOptions>
template <class EigenType, class Handle>
inline
const Handle&
EigenTypes<MapOptions>::HandleMap<EigenType, Handle>::memoryHandle() const {
    return mMemoryHandle;
}

#endif // MADLIB_EIGEN_LINALGTYPES_HPP (workaround for Doxygen)
