/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyType_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Class for representing any type.
 *
 * Instances of this class act a proxy to any arbitrary ConcreteType<T>.
 */
class AnyType : public AbstractType {
public:
    class iterator;
    
    /**
     * @internal
     * This constructor will only be generated if \c T is not a subclass of
     * \c AbstractType (in which case the other constructor will be used).
     * We use boost's \c enable_if here, because for subclasses of AbstractType
     * this template constructor would take precedence over the constructor
     * having AbstractType as argument.
     */
//    template <typename T>
//    AnyType(const T &inValue,
//        typename disable_if<boost::is_base_of<AbstractType, T> >::type *dummy = NULL)

    /**
     * @brief Template conversion constructor
     *
     * @note When overload resolution takes place, a template function has lower
     *       precedence than non-template functions:
     *       http://publib.boulder.ibm.com/infocenter/compbgpl/v9v111/topic/com.ibm.xlcpp9.bg.doc/language_ref/overloading_function_templates.htm
     */
    template <typename T>
    AnyType(const T &inValue)
        : mDelegate(new ConcreteType<T>(inValue)) { }
    
    /**
     *
     */
    AnyType(AbstractTypeSPtr inDelegate) : mDelegate(inDelegate) { }

    /**
     * This constructor will only by called if inValue is *not* of type
     * AnyType (in which case the copy constructor would be called
     * instead).
     */
//    AnyType(const AbstractType &inValue) : mDelegate(inValue) {
//    }
    
    /**
     * @brief The copy constructor: Perform a shallow copy, i.e., copy only
     *        reference to delegate
     */
    AnyType(const AnyType &inValue) : mDelegate(inValue.mDelegate) { };
        
    /**
     * We want to allow the notation anyType = Null();
     */
    AnyType(const Null &inValue) { }

    /**
     * @brief Template conversion operator
     *
     * @internal
     *     A template conversion operator is not without issues. In particular,
     *     non-sense operations are now syntactically correct. E.g.,
     *     if (anyType1 == anyType2)
     */
    template <class T>
    operator T() const;
    
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
    const AnyType &operator[](uint16_t inID) const {
        return getValueByID(inID);
    }
    
    /**
     * @brief The default implementation returns an empty smart pointer
     */
    const AnyType &getValueByID(uint16_t inID) const {
        if (mDelegate) {
            if (mDelegate->isCompound() && inID < mDelegate->size())
                return dynamic_cast<const AnyType&>(mDelegate->getValueByID(inID));
            else if (inID == 0)
                return *this;
        }
        
        if (inID != 0)
            throw std::out_of_range("Tried to access non-zero index of non-tuple type");
        
        return *this;
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
     * @internal Cannot be instantiated directly.
     */
    AnyType() {
    }
        
private:    
    AbstractTypeSPtr mDelegate;
};
