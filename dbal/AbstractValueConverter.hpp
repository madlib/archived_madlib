/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractValueConverter.hpp
 *
 * @brief Header file for value-converter interface
 *        (a callback used by ConcreteValue)
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_ABSTRACTVALUECONVERTER_HPP
#define MADLIB_ABSTRACTVALUECONVERTER_HPP

#include <madlib/dbal/dbal.hpp>


namespace madlib {

namespace dbal {

class AbstractValueConverter {
public:
    #define EXPAND_TYPE(T) \
        virtual void convert(const T &inValue) = 0;
    
    EXPAND_FOR_ALL_TYPES
    
    #undef EXPAND_TYPE
};

} // namespace dbal

} // namespace madlib

#endif
