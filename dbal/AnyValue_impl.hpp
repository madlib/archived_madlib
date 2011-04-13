/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyValue_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Try to convert into whatever type is requested
 */
// FIXME: Think whether we need to use a safe conversion idiom like
// http://www.artima.com/cppsource/safebool.html
template <class T>
AnyValue::operator T() const {
    AbstractValueSPtr lastDelegate;
    
    if (!mDelegate || !(lastDelegate = mDelegate->getValueByID(0)))
        throw std::logic_error("Internal type conversion error");
    
    return lastDelegate->getAs( static_cast<T*>(NULL) /* ignored type parameter */ );
}
