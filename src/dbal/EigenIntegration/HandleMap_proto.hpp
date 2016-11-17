/* ----------------------------------------------------------------------- *//**
 *
 * @file HandleMap_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_EIGEN_HANDLEMAP_PROTO_HPP
#define MADLIB_DBAL_EIGEN_HANDLEMAP_PROTO_HPP

namespace madlib {

namespace dbal {

namespace eigen_integration {

/**
 * @brief Wrapper class for linear-algebra types based on Eigen
 */
template <class EigenType, class Handle, int MapOptions = Eigen::Unaligned>
class HandleMap : public Eigen::Map<EigenType, MapOptions> {
public:
    typedef EigenType PlainEigenType;
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
     * @brief Initialize HandleMap backed by the given handle without dimension
     * information
     *
     * We do not assume that Handle has member functions other than ptr(),
     * e.g. TransparentHandle does not have size(). So we set dummy dimension
     * information here.
     */
    inline HandleMap(const Handle &inHandle)
      : Base(const_cast<Scalar*>(inHandle.ptr()), 1, 1),
        mMemoryHandle(inHandle) { }

     /*
     * FIXME These two functions do not work when Handle cannot be constructed
     * by a single argument of type Scalar*, e.g. they work for
     * Handle=TransparentHanle (MappedColumnVector, etc.),
     * but not Handle=ArrayHandle (NativeMatrix, etc.).
     */
    /**
     * @brief Initialize HandleMap using Eigen mapped expressions
     *
     * For example, this allows construction of MappedColumnVector from
     * MappedMatrix::col(int) or NativeColumnVector, etc.
     */
    template <class Derived>
    HandleMap(const Eigen::MapBase<Derived>& inMappedData,
        typename boost::enable_if_c<Derived::IsVectorAtCompileTime>::type* = 0)
      : Base(inMappedData.data(), inMappedData.size()),
        mMemoryHandle(inMappedData.data()) {

        madlib_assert(inMappedData.innerStride() == 1,
            std::runtime_error("HandleMap does not support non-contiguous "
                "memory blocks."));
    }

    template <class Derived>
    HandleMap(const Eigen::MapBase<Derived>& inMappedData,
        typename boost::enable_if_c<!Derived::IsVectorAtCompileTime>::type* = 0)
      : Base(inMappedData.data(), inMappedData.rows(), inMappedData.cols()),
        mMemoryHandle(inMappedData.data()) {

        madlib_assert(inMappedData.innerStride() == 1 &&
            ((Derived::IsRowMajor && inMappedData.outerStride() == inMappedData.cols())
            || (!Derived::IsRowMajor && inMappedData.outerStride() == inMappedData.rows())),
            std::runtime_error("HandleMap does not support non-contiguous "
                "memory blocks."));
    }

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

    HandleMap& operator=(const HandleMap& other);
    HandleMap& rebind(const Handle &inHandle);
    HandleMap& rebind(const Handle &inHandle, Index inSize);
    HandleMap& rebind(const Handle &inHandle, Index inRows, Index inCols);
    HandleMap& rebind(Index inSize);
    HandleMap& rebind(Index inRows, Index inCols);
    const Handle &memoryHandle() const;
    bool isNull() const { return mMemoryHandle.isNull(); }

protected:
    Handle mMemoryHandle;

private:
    BOOST_STATIC_ASSERT_MSG(
        Handle::isMutable || boost::is_const<EigenType>::value,
        "non-const matrix cannot be backed by immutable handle");
};

} // namespace eigen_integration

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_DBAL_EIGEN_HANDLEMAP_PROTO_HPP)
