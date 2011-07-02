/* ----------------------------------------------------------------------- *//**
 *
 * @file ValueConverter_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

template <typename T>
inline ValueConverter<T>::ValueConverter(const AbstractValue &inValue)
: mValue(inValue), mConvertedValue(), mDatumInitialized(false) { }

/**
 * @brief Convert the AbstractValue object passed to the constructor to the type
 *        specified in the template argument
 *
 * This function calls AbstractValue::convert from which the appropriate
 * ValueConverter<T>::convert function is called. (We are using the vtable
 * instead of a big if-then-else block.)
 */
template <typename T>
inline ValueConverter<T>::operator T() {
    if (!mDatumInitialized) {
        // This does a call-back to our convert(), overloaded for all
        // ConcreteValue<...> subclasses
        mValue.convert(*this);
        mDatumInitialized = true;
    }
    
    return mConvertedValue;
}
