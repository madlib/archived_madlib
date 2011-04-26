/* ----------------------------------------------------------------------- *//**
 *
 * @file AnyValue_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

class AnyValue : public AbstractValue {
public:
    class iterator : public std::iterator<std::input_iterator_tag, AnyValue> {
    public:
        iterator(const AbstractValue &inValue)
            : mValue(inValue), mCurrentID(0) {
        }
        
        iterator &operator++() {
            ++mCurrentID;
            return *this;
        }
        
        // Post-increment operator
        iterator operator++(int) {
            iterator temp(*this);
            operator++();
            return temp;
        }
        
        bool operator==(const iterator &inRHS) {
            return mCurrentID == inRHS.mCurrentID;
        }
        
        bool operator!=(const iterator &inRHS) {
            return mCurrentID != inRHS.mCurrentID;
        }
        
        bool operator<(const iterator &inRHS) {
            return mCurrentID < inRHS.mCurrentID;
        }
        
        AnyValue operator*() {
            // FIXME: This could be more efficient, operator->() creates AnyValue
            // which is then deleted again
            return *this->operator->();
        }
        
        shared_ptr<const AnyValue> operator->() {
            AbstractValueSPtr valuePtr = mValue.getValueByID(mCurrentID);
            shared_ptr<const AnyValue> anyValuePtr(
                dynamic_pointer_cast<const AnyValue>( valuePtr ));
            
            if (anyValuePtr)
                return anyValuePtr;
            else
                // Return an AnyValue by value (which uses the Value returned
                // by getValueByID as delegate)
                return shared_ptr<AnyValue>( new AnyValue(valuePtr) );
        }
            
    private:
        const AbstractValue &mValue;
        int mCurrentID;
    };

    /**
     * @internal
     * This constructor will only be generated if \c T is not a subclass of
     * \c AbstractValue (in which case the other constructor will be used).
     * We use boost's \c enable_if here, because for subclasses of AbstractValue
     * this template constructor would take precedence over the constructor
     * having AbstractValue as argument.
     */
    template <typename T>
    AnyValue(const T &inValue,
        typename disable_if<boost::is_base_of<AbstractValue, T> >::type *dummy = NULL)
        : mDelegate(new ConcreteValue<T>(inValue)) { }
    
    /**
     * This copy constructor will only by called if inValue is *not* of type
     * AnyValue (in which case the implicit copy constructor would be called
     * instead). This behavior is desired here!
     */
    AnyValue(const AbstractValue &inValue) : mDelegate(inValue.clone()) {
    }
    
    /**
     * We want to allow the notation anyValue = Null();
     */
    AnyValue(const Null &inValue) { }

    template <class T>
    operator T() const;
    
    bool isCompound() const {
        if (mDelegate)
            return mDelegate->isCompound();
        
        return AbstractValue::isCompound();
    }
            
    bool isNull() const {
        return !mDelegate;
    }
    
    unsigned int size() const {
        if (mDelegate)
            return mDelegate->size();
        
        return AbstractValue::size();
    }
    
    AnyValue copyIfImmutable() const {
        // FIXME: Necessary to test: if (mDelegate)?
        if (mDelegate->isMutable())
            return *this;
        
        return AnyValue( mDelegate->mutableClone() );
    }

    void convert(AbstractValueConverter &inConverter) const {
        if (mDelegate)
            mDelegate->convert(inConverter);
    }
    
    AnyValue operator[](uint16_t inID) {
        AbstractValueSPtr valuePtr = getValueByID(inID);
        shared_ptr<const AnyValue> anyValuePtr(
            dynamic_pointer_cast<const AnyValue>( valuePtr ));

        // FIXME: Null!
        if (anyValuePtr)
            return *anyValuePtr;
        else
            // Return an AnyValue by value (which uses the Value returned
            // by getValueByID as delegate)
            return AnyValue(valuePtr);
    }
    
protected:
    /**
     * Cannot be instantiated directly.
     */
    AnyValue() {
    }
    
    AbstractValueSPtr clone() const {
        return AbstractValueSPtr( new AnyValue(*this) );
    }
        
    /**
     * The default implementation returns an empty smart pointer
     */
    AbstractValueSPtr getValueByID(unsigned int inID) const {
        if (mDelegate)
            return mDelegate->getValueByID(inID);
        
        return AbstractValueSPtr();
    };

private:
    /**
     * Constructor is private -- it is only used by the friend class Record, in
     * Record::getValueByID(). Derived classes cannot call this constructor.
     */
    AnyValue(AbstractValueSPtr inDelegate) : mDelegate(inDelegate) { }
    
    AbstractValueSPtr mDelegate;
};
