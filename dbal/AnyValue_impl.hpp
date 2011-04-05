/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyValue_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

template <class T>
AnyValue::operator T() const {
    AbstractValueSPtr lastDelegate;
    
    if (!mDelegate || !(lastDelegate = mDelegate->getValueByID(0)))
        throw std::logic_error("Internal type conversion error");
    
    return lastDelegate->getAs( static_cast<T*>(NULL) /* ignored type parameter */ );
}
