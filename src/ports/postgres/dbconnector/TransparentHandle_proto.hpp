/* ----------------------------------------------------------------------- *//**
 *
 * @file TransparentHandle_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_TRANSPARENTHANDLE_PROTO_HPP
#define MADLIB_POSTGRES_TRANSPARENTHANDLE_PROTO_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

/**
 * @brief Handle without any meta data (essentially, a constant pointer)
 *
 * A TransparentHandle is simply a (constant) pointer. It is used whenever we
 * need a type that conforms to the handle policy, but no meta data is required.
 */
template <typename T, bool IsMutable = false>
class TransparentHandle {
public:
    typedef const T val_type;
    enum { isMutable = IsMutable };

    TransparentHandle(val_type* inPtr);

    val_type* ptr() const;
    bool isNull() const { return mPtr == NULL; }

protected:
    val_type *mPtr;
};

/**
 * @brief Mutable handle without any meta data (essentially, a pointer)
 */
template <typename T>
class TransparentHandle<T, dbal::Mutable>
  : public TransparentHandle<T, dbal::Immutable> {

public:
    typedef TransparentHandle<T, dbal::Immutable> Base;
    typedef T val_type;
    enum { isMutable = dbal::Mutable };

    TransparentHandle(val_type* inPtr);

    // Import the const version as well
    using Base::ptr;
    using Base::isNull;
    val_type* ptr();

protected:
    using Base::mPtr;
};

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_TRANSPARENTHANDLE_PROTO_HPP)
