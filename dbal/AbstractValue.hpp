/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractValue.hpp
 *
 * @brief Header file for abstract value class
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_ABSTRACTVALUE_HPP
#define MADLIB_ABSTRACTVALUE_HPP

#include <madlib/dbal/dbal.hpp>


namespace madlib {

namespace dbal {

class AbstractValue {
    friend class AnyValue;
//    friend class ValueConverter;

public:
    virtual unsigned int size() const {
        return 1;
    }
    
    virtual bool isCompound() const {
        return false;
    }
    
    virtual AbstractValueSPtr operator[](unsigned int inID) const {
        return getValueByID(inID);
    }
    
    virtual bool isNull() const {
        return false;
    }

    /**
     * This function performs a callback to the specified ValueConverter.
     * This allows relying on the vtable of ValueConverter for dispatching
     * conversion requests.
     */
    virtual void convert(AbstractValueConverter &inConverter) const {
    }
    
protected:
    AbstractValue() {
    }
    
    virtual AbstractValueSPtr getValueByID(unsigned int inID) const {
        return AbstractValueSPtr();
    }
    
    virtual AbstractValueSPtr clone() const {
        return AbstractValueSPtr();
    }
};

} // namespace dbal

} // namespace madlib

#endif
