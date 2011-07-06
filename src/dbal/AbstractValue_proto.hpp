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
 * Instances of AbstractValue can be recursive tree structures. In this case,
 * values are called \em compounds and are made up of several elements. Each of
 * these elements is again of type AbstractValue (and can again be a compound).
 */
class AbstractValue {
    friend class AnyValue;

public:
    virtual ~AbstractValue() { }

    /**
     * @brief Return the number of elements in this compound value (only
     *     counting direct children)
     */
    virtual unsigned int size() const {
        return 1;
    }
    
    /**
     * @brief Return whether this variable contains a true compound value (a
     *     record in SQL terminology, equivalent to a \c struct in C).
     */
    virtual bool isCompound() const {
        return false;
    }
    
    /**
     * @brief Return whether this variable is Null (as in SQL, do not confuse
     *     with NULL pointers in C).
     */
    virtual bool isNull() const {
        return false;
    }
    
    /**
     * @brief Return whether this variable is mutable. Modifications to an
     *     immutable variable will cause an exception.
     *
     * @internal Immutable variables are key to avoiding unnecessary copying of
     *     internal data structures.
     */
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
    
    /**
     * @brief Get the element at the given position (0-based).
     */
    virtual AbstractValueSPtr getValueByID(unsigned int inID) const {
        return AbstractValueSPtr();
    }
    
    /**
     * @brief Return a deep copy of this variable.
     */
    virtual AbstractValueSPtr clone() const {
        return AbstractValueSPtr();
    }
    
    /**
     * @brief Return a mutable copy of this variable.
     */
    virtual AbstractValueSPtr mutableClone() const {
        return AbstractValueSPtr();
    }
};
