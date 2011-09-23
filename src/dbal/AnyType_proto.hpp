/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyType_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Class for representing any type.
 *
 * Instances of this class act a proxy for any arbitrary \ref ConcreteType.
 */
class AnyType : public AbstractType {
    // ConcreteType<AnyTypeVector>::getValueByID() accesses mDelegate, so we
    // make it a friend
    friend class ConcreteType<AnyTypeVector>;

public:
    class iterator;
    
    typedef std::back_insert_iterator<AnyTypeVector> insertIterator;
    
    /**
     * @brief Conversion constructors
     */        
    #define EXPAND_TYPE(T) \
        AnyType(const T &inValue);

    EXPAND_FOR_ALL_TYPES

    #undef EXPAND_TYPE
    
    /**
     * @brief Constructor for initalization with delegate
     */
    AnyType(AbstractTypeSPtr inDelegate) : mDelegate(inDelegate) { }

    /**
     * @brief The copy constructor: Perform a shallow copy, i.e., copy only
     *        reference to delegate
     */
    AnyType(const AnyType &inValue)
        : AbstractType(inValue), mDelegate(inValue.mDelegate) { }
        
    /**
     * @brief Constructor for initializing as Null
     */
    AnyType(const Null & /* inValue */) { }

    /**
     * @brief Conversion operators
     *
     * @internal
     *     Conversion operators for primitive data types are not without issues.
     *     In particular, non-sense operations are now syntactically correct.
     *     E.g., if (anyType1 == anyType2)
     */
    #define EXPAND_TYPE(T) \
        operator T() const;

    EXPAND_FOR_ALL_TYPES

    #undef EXPAND_TYPE

    
    bool isCompound() const {
        if (mDelegate)
            return mDelegate->isCompound();
        
        return AbstractType::isCompound();
    }
    
    bool isNull() const {
        return !mDelegate;
    }
    
    bool isMutable() const {
        if (mDelegate)
            return mDelegate->isMutable();
        
        return AbstractType::isMutable();
    }
    
    unsigned int size() const {
        if (mDelegate)
            return mDelegate->size();
        
        return AbstractType::size();
    }
    
    /**
     * @brief Get the element at the given position (0-based).
     *
     * This function is a convenience wrapper around getValueByID(uint16_t).
     */
    AnyType operator[](uint16_t inID) const {
        return AnyType(getValueByID(inID));
    }
    
    /**
     * @brief The default implementation returns an empty smart pointer
     */
    AbstractTypeSPtr getValueByID(uint16_t inID) const {
        if (mDelegate)
            return mDelegate->getValueByID(inID);
            
        return AbstractTypeSPtr();
    }

    /**
     * @brief Clone this instance if it is not mutable, otherwise return this
     */
    AnyType cloneIfImmutable() const {
        if (isMutable() || !mDelegate)
            return *this;
        
        return AnyType(mDelegate->clone());
    }

    void performCallback(AbstractTypeConverter &inConverter) const {
        if (mDelegate)
            return mDelegate->performCallback(inConverter);
        
        return AbstractType::performCallback(inConverter);
    }
        
    /**
     * @brief Return a mutable copy of this variable.
     */
    AbstractTypeSPtr clone() const {
        if (mDelegate)
            return AbstractTypeSPtr(new AnyType(mDelegate->clone()));
        
        return AbstractTypeSPtr(new AnyType(Null()));
    }
    
    
protected:
    /**
     * @brief Protected default constructor
     *
     * We do not want AnyType to be used without explicit initalization in user
     * code.
     */
    AnyType() {
    }
        
private:    
    AbstractTypeSPtr mDelegate;
};
