/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractValueConverter.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Interface for value-conversion call back
 *
 * This class declares a conversion functions for all types supported by the
 * MADlib DB abstraction layer.
 */
class AbstractValueConverter {
public:
    virtual ~AbstractValueConverter() { }

    #define EXPAND_TYPE(T) \
        virtual void convert(const T &inValue) = 0;
    
    EXPAND_FOR_ALL_TYPES
    
    #undef EXPAND_TYPE
};
