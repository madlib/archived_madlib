/* ----------------------------------------------------------------------- *//**
 *
 * @file HandleMap_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by EigenLinAlgTypes.hpp
#ifdef MADLIB_EIGEN_LINALGTYPES_HPP

/**
 * @brief Wrapper class for linear-algebra types based on Eigen
 */
template <int MapOptions>
template <class EigenType, class Handle>
class EigenTypes<MapOptions>::HandleMap
  : public Eigen::Map<EigenType, MapOptions> {
public:
    typedef Eigen::Map<EigenType, MapOptions> Base;
    typedef typename Base::Scalar Scalar;
    typedef typename Base::Index Index;
    
    using Base::operator=;

    /**
     * @brief Default constructor
     *
     * Using the HandleMap before a rebind() is undefined and will likely
     * crash the application.
     */
    inline HandleMap()
      : Base(NULL, 1, 1), mMemoryHandle(NULL) { }

    /**
     * @brief Initialize HandleMap backed by the given handle
     *
     * This constructor requires that Handle has a size() method, which is
     * called to determine the length of the vector.
     */
    inline HandleMap(const Handle &inHandle)
      : Base(const_cast<Scalar*>(inHandle.ptr()), inHandle.size()),
        mMemoryHandle(inHandle) { }

    /**
     * @brief Initialize HandleMap backed by the given handle
     *
     * This ignores any size information Handle may have. This
     * constructor can be used with any Handle.
     */
    HandleMap(const Handle &inHandle, Index inNumElem)
      : Base(const_cast<Scalar*>(inHandle.ptr()), inNumElem),
        mMemoryHandle(inHandle) { }
    
    /**
     * @brief Initialize HandleMap backed by the given handle
     *
     * This ignores any size information Handle may have. This
     * constructor can be used with any Handle.
     *
     * @internal
     *     We need to do a const cast here: There is no alternative to
     *     initializing from "const Handle&", but the base class constructor
     *     needs a non-const value. We therefore make sure that a
     *     non-mutable handle can only be assigned if EigenType is a
     *     constant type. See the static assert below.
     */
    HandleMap(const Handle &inHandle, Index inNumRows, Index inNumCols)
      : Base(const_cast<Scalar*>(inHandle.ptr()), inNumRows, inNumCols),
        mMemoryHandle(inHandle) { }
    
    operator AbstractionLayer::AnyType() const;
    HandleMap& operator=(const HandleMap& other);
    HandleMap& rebind(const Handle &inHandle);
    HandleMap& rebind(const Handle &inHandle, Index inSize);
    HandleMap& rebind(const Handle &inHandle, Index inRows, Index inCols);
    const Handle &memoryHandle() const;

protected:
    Handle mMemoryHandle;
    
private:
    BOOST_STATIC_ASSERT_MSG(
        Handle::isMutable || boost::is_const<EigenType>::value,
        "non-const matrix cannot be backed by immutable handle");
};

#endif // MADLIB_EIGEN_LINALGTYPES_HPP (workaround for Doxygen)
