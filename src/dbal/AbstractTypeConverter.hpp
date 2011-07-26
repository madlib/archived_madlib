/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractTypeConverter.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Interface for value-conversion call back
 *
 * This class declares a conversion function for all types supported by the
 * MADlib DB abstraction layer.
 */
class AbstractTypeConverter {
public:
    virtual ~AbstractTypeConverter() = 0;

    #define EXPAND_TYPE(T) \
        void callbackWithValue(const T & /* inValue */ ) { \
            throwError(); \
        }
    
    EXPAND_FOR_ALL_TYPES
    
    #undef EXPAND_TYPE

private:
    void throwError() {
        throw std::logic_error("Internal type conversion error");
    }
};

// We need to provide an implementation
inline AbstractTypeConverter::~AbstractTypeConverter() { }
