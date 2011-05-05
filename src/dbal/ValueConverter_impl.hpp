/* ----------------------------------------------------------------------- *//**
 *
 * @file ValueConverter_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

template <typename T>
inline ValueConverter<T>::ValueConverter(const AbstractValue &inValue)
: mValue(inValue), mConvertedValue(), mDatumInitialized(false) { }

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
