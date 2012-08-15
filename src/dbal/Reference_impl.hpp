/* ----------------------------------------------------------------------- *//**
 *
 * @file Reference_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_REFERENCE_IMPL_HPP
#define MADLIB_DBAL_REFERENCE_IMPL_HPP

namespace madlib {

namespace dbal {

// Ref<T, IsMutable>

template <typename T, bool IsMutable>
Ref<T, IsMutable>::Ref()
  : mPtr(NULL) { }

template <typename T, bool IsMutable>
Ref<T, IsMutable>::Ref(val_type* inPtr)
  : mPtr(inPtr) { }

template <typename T, bool IsMutable>
Ref<T, IsMutable>&
Ref<T, IsMutable>::rebind(val_type* inPtr) {
    mPtr = inPtr;
    return *this;
}

template <typename T, bool IsMutable>
Ref<T, IsMutable>::operator val_type&() const {
    return *mPtr;
}

template <typename T, bool IsMutable>
typename Ref<T, IsMutable>::val_type*
Ref<T, IsMutable>::ptr() const {
    return mPtr;
}

template <typename T, bool IsMutable>
bool
Ref<T, IsMutable>::isNull() const {
    return mPtr == NULL;
}

/**
 * @brief Copy operator is defined but protected, in order to prevent usage.
 */
template <typename T, bool IsMutable>
Ref<T, IsMutable>&
Ref<T, IsMutable>::operator=(const Ref& inRef) {
    return *this;
}


// Ref<T, true /* IsMutable */>

template <typename T>
Ref<T, true>::Ref() : Base() { }

template <typename T>
Ref<T, true>::Ref(val_type* inPtr) : Base(inPtr) { }

template <typename T>
Ref<T, true>&
Ref<T, true>::rebind(val_type* inPtr) {
    Base::rebind(inPtr);
    return *this;
}

template <typename T>
Ref<T, true>::operator const val_type&() const {
    return Base::operator const val_type&();
}

template <typename T>
Ref<T, true>::operator val_type&() {
    return const_cast<val_type&>(
        static_cast<Base*>(this)->operator const val_type&()
    );
}

/**
 * @internal
 * It is important to define this operator because C++ will otherwise
 * perform an assignment as a bit-by-bit copy. Note that this default
 * operator= would be used even though there is a conversion path
 * through dest.operator=(orig.operator const val_type&()).
 */
template <typename T>
Ref<T, true>&
Ref<T, true>::operator=(Ref<T, true>& inRef) {
    *const_cast<val_type*>(mPtr) = inRef;
    return *this;
}

template <typename T>
Ref<T, true>&
Ref<T, true>::operator=(const val_type& inValue) {
    *const_cast<val_type*>(mPtr) = inValue;
    return *this;
}

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_DBAL_REFERENCE_IMPL_HPP)
