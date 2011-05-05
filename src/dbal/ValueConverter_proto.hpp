/* ----------------------------------------------------------------------- *//**
 *
 * @file ValueConverter_proto.hpp
 *
 *//* ----------------------------------------------------------------------- */

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
