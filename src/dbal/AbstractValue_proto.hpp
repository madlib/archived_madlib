/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractValue_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Abstract base class for in- and output types of module functions
 *
 * This class provides the interface that MADlib modules use for accessing input
 * and returning output values.
 *
 * Subclasses of AbstractValue can be recursive structures. In this case, values
 * are called \em compounds and are made up of several elements. Each of these
 * elements is again an AbstractValue (and can again be a compound).
 */
class AbstractValue {
    friend class AnyValue;

public:
    virtual ~AbstractValue() { }

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
     * @brief Call AbstractValueConverter::convert() with this as argument
     *
     * This function performs a callback to the specified ValueConverter.
     * This allows relying on the vtable of AbstractValueConverter for
     * dispatching conversion requests.
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
