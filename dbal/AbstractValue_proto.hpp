/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractValue_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

class AbstractValue {
    friend class AnyValue;

public:
    virtual unsigned int size() const {
        return 1;
    }
    
    virtual bool isCompound() const {
        return false;
    }
    
    virtual bool isNull() const {
        return false;
    }
    
    virtual bool isMutable() const {
        return true;
    }
        
    /**
     * This function performs a callback to the specified ValueConverter.
     * This allows relying on the vtable of ValueConverter for dispatching
     * conversion requests.
     */
    virtual void convert(AbstractValueConverter &inConverter) const {
    }
    
    #define EXPAND_TYPE(T) \
        inline virtual T getAs(T* /* pure type parameter */) const;

    EXPAND_FOR_ALL_TYPES

    #undef EXPAND_TYPE
    
protected:
    AbstractValue() {
    }
    
    virtual AbstractValueSPtr getValueByID(unsigned int inID) const {
        return AbstractValueSPtr();
    }
    
    virtual AbstractValueSPtr clone() const {
        return AbstractValueSPtr();
    }
    
    virtual AbstractValueSPtr mutableClone() const {
        return AbstractValueSPtr();
    }
};
