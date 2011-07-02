/* ----------------------------------------------------------------------- *//**
 *
 * @file ValueConverter_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief The default Value-conversion callback.
 *
 * This class is initialized with an AbstractValue. It provides conversion to
 * the type specified as template argument. It does so by calling
 * AbstractValue::convert and providing this instance as callback object.
 * In detail, AbstractValue::convert will call back convert(const T&). In
 * essence, we are using the vtable for dynamic type dispatching.
 */
template <typename T>
class ValueConverter : public AbstractValueConverter {
public:
    ValueConverter(const AbstractValue &inValue);
    
    operator T();
    
    #define EXPAND_TYPE(T) \
        void convert(const T &inValue) { \
            throwError(); \
        }
    
    EXPAND_FOR_ALL_TYPES
    
    #undef EXPAND_TYPE
    
protected:
    const AbstractValue &mValue;
    T mConvertedValue;
    bool mDatumInitialized;

private:
    void throwError() {
        throw std::logic_error("Internal type conversion error");
    }
};
