/* ----------------------------------------------------------------------- *//**
 *
 * @file ConcreteValue_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Template class that wraps arbitrary types
 *
 * ConcreteValue<T> implements the AbstractType interface. It can contain
 * values of arbitrary type.
 * 
 * @internal The main benefit of wrapping arbitrary types with this class is to
 *     support NULL values and providing a unified interface for both primitive
 *     types and composite types.
 */
template <typename T>
class ConcreteType : public AbstractType {
public:
    typedef std::back_insert_iterator<T> iterator;

    ConcreteType() { }
    ConcreteType(const T &inValue);

    void performCallback(AbstractTypeConverter &inConverter) const {
        inConverter.callbackWithValue(mValue);
    }
    
    inline unsigned int size() const {
        return AbstractType::size();
    }
    
    inline bool isCompound() const {
        return AbstractType::isCompound();
    }
    
    /**
     * @brief Return whether this variable is mutable. Modifications to an
     *     immutable variable will cause an exception.
     *
     * This function must be specialized for immutable types.
     */
    inline bool isMutable() const {
        return true;
    }

    AbstractTypeSPtr getValueByID(uint16_t inID) const;

    const T &get() const {
        return mValue;
    }
            
    #define EXPAND_TYPE(T) \
        inline T getAs(T* ignoredTypeParameter) const { \
            return AbstractType::getAs(ignoredTypeParameter); \
        }

    EXPAND_FOR_ALL_TYPES

    #undef EXPAND_TYPE

    /**
     * @brief Return a mutable copy of this variable.
     *
     * This function will be specialized for immutable types.
     */
    AbstractTypeSPtr clone() const {
        return AbstractTypeSPtr(new ConcreteType(*this));
    }


protected:
    
protected:
    const T mValue;
};
