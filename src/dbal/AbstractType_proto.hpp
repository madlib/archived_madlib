/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractType_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Abstract base class for in- and output types of module functions
 *
 * This class provides the interface that MADlib modules use for accessing input
 * and returning output values.
 *
 * Instances of AbstractType can be recursive tree structures. In this case,
 * values are called \em compounds and are made up of several elements. Each of
 * these elements is again of type AbstractType (and can again be a compound).
 */
class AbstractType {
public:
    virtual ~AbstractType() { }

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
     * @brief Get the element at the given position (0-based).
     */
    virtual AbstractTypeSPtr getValueByID(uint16_t inID) const = 0;
    
    /**
     * @brief Return a mutable copy of this variable.
     *
     * A copy is \em mutable if is entirely represented with memory that is
     * allowed to be changed. This is not necessarily a deep copy.
     * Note: Overrides use a covariant return type.
     */
    virtual AbstractTypeSPtr clone() const = 0;

    /**
     * @brief Call AbstractTypeConverter::convert() with this as argument
     *
     * This function performs a callback to the specified TypeConverter.
     * This allows relying on the vtable of AbstractTypeConverter for
     * dispatching conversion requests.
     */
    virtual void performCallback(AbstractTypeConverter & /* inConverter */)
        const { }
    
    #define EXPAND_TYPE(T) \
        inline virtual T getAs(T* /* pure type parameter */) const;

    EXPAND_FOR_ALL_TYPES

    #undef EXPAND_TYPE
    
protected:
    AbstractType() {
    }
};
