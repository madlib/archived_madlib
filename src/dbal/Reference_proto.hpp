/* ----------------------------------------------------------------------- *//**
 *
 * @file Reference_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_REFERENCE_PROTO_HPP
#define MADLIB_DBAL_REFERENCE_PROTO_HPP

namespace madlib {

namespace dbal {

/**
 * @brief Reference with changeable target
 *
 * @tparam T Target type of reference
 * @tparam IsMutable Whether the target value is mutable. Note that \c IsMutable
 *     overrides any const-qualifier that \c T may contain.
 */
template <typename T, bool IsMutable = boost::is_const<T>::value_type>
class Ref {
public:
    typedef const T val_type;
    enum { isMutable = IsMutable };

    Ref();
    Ref(val_type* inPtr);

    Ref& rebind(val_type* inPtr);
    operator val_type&() const;
    val_type* ptr() const;
    bool isNull() const;

protected:
    Ref& operator=(const Ref& inRef);

    val_type* mPtr;
};

template <typename T>
class Ref<T, true /* IsMutable */>
  : public Ref<T, false> {

public:
    typedef Ref<T, false> Base;
    typedef T val_type;
    enum { isMutable = true };

    Ref();
    Ref(val_type* inPtr);

    Ref& rebind(val_type* inPtr);
    operator const val_type&() const;
    operator val_type&();
    using Base::ptr;
    val_type* ptr();
    Ref& operator=(Ref& inRef);
    Ref& operator=(const val_type& inValue);

protected:
    using Base::mPtr;


};

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_DBAL_REFERENCE_PROTO_HPP)
