/* ----------------------------------------------------------------------- *//**
 *
 * @file ConcreteValue_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

template <typename T>
class ConcreteValue : public AbstractValue {
    friend class AnyValue;

public:
    typedef std::back_insert_iterator<T> iterator;

    ConcreteValue()
        : mIsNull(true) { }
    ConcreteValue(const T &inValue);

    void convert(AbstractValueConverter &inConverter) const {
        inConverter.convert(mValue);
    }
    
    inline unsigned int size() const {
        return AbstractValue::size();
    }
    
    inline bool isCompound() const {
        return AbstractValue::isCompound();
    }
    
    /**
     * This function will be specialized for immutable types.
     */
    inline bool isMutable() const {
        return true;
    }

    const T &get() const {
        return mValue;
    }
    
    #define EXPAND_TYPE(T) \
        inline T getAs(T* ignoredTypeParameter) const { \
            return AbstractValue::getAs(ignoredTypeParameter); \
        }

    EXPAND_FOR_ALL_TYPES

    #undef EXPAND_TYPE


protected:
    AbstractValueSPtr getValueByID(unsigned int inID) const;

    AbstractValueSPtr clone() const {
        return AbstractValueSPtr( new ConcreteValue<T>(*this) );
    }
    
    /**
     * This function will be specialized for immutable types.
     */
    AbstractValueSPtr mutableClone() const {
        return clone();
    }

protected:
    const T mValue;
    const bool mIsNull;
};
