/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractValueConverter.hpp
 *
 *//* ----------------------------------------------------------------------- */

class AbstractValueConverter {
public:
    #define EXPAND_TYPE(T) \
        virtual void convert(const T &inValue) = 0;
    
    EXPAND_FOR_ALL_TYPES
    
    #undef EXPAND_TYPE
};
