/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyValue.cpp
 *
 *//* ----------------------------------------------------------------------- */

#include <madlib/dbal/ValueConverter.hpp>

namespace madlib {

namespace dbal {

// Because of separate compilation we have to instantiate the template for all
// needed types

#define EXPAND_TYPE(T) \
    template AnyValue::operator T() const;

EXPAND_FOR_ALL_TYPES

#undef EXPAND_TYPE

template <class T>
AnyValue::operator T() const {
//        shared_ptr<const ConcreteValue<T> > delegate(
//            dynamic_pointer_cast<const ConcreteValue<T> >(mDelegate));

//        if (!delegate)
//            throw std::logic_error("Internal type conversion error");
    AbstractValueSPtr lastDelegate;
    
    if (!mDelegate || !(lastDelegate = mDelegate->getValueByID(0)))
        throw std::logic_error("Internal type conversion error");
    
    return ValueConverter<T>(*lastDelegate);
}

} // namespace dbal

} // namespace madlib
