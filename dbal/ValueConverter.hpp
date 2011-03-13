/* ----------------------------------------------------------------------- *//**
 *
 * @file ValueConverter.hpp
 *
 * @brief Header file for value-converter interface
 *        (a callback used by ConcreteValue)
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_VALUECONVERTER_HPP
#define MADLIB_VALUECONVERTER_HPP

#include <madlib/dbal/dbal.hpp>
#include <madlib/dbal/AbstractValue.hpp>
#include <madlib/dbal/AbstractValueConverter.hpp>
#include <madlib/dbal/AnyValue.hpp>

#include <stdexcept>
#include <vector>

namespace madlib {

namespace dbal {

template <typename T>
class ValueConverter : AbstractValueConverter {
public:
    ValueConverter(const AbstractValue &inValue)
        : mValue(inValue), mConvertedValue(), mDatumInitialized(false) { };
    
    operator T() {
        if (!mDatumInitialized) {
            // This does a call-back to our convert(), overloaded for all
            // ConcreteValue<...> subclasses
            mValue.convert(*this);
            mDatumInitialized = true;
        }
        
        return mConvertedValue;
    }
    
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

// If imcplicit conversion is lossless, we want to allow it
#ifdef DECLARE_OR_DEFINE_STD_CONVERSION
    #error "DECLARE_OR_DEFINE_STD_CONVERSION was declared before."
#endif
#define DECLARE_OR_DEFINE_STD_CONVERSION(x,y) \
    template <> \
    void ValueConverter<y>::convert(const x &inValue)

#define EXPAND_TYPE(T) \
    DECLARE_OR_DEFINE_STD_CONVERSION(T, T);

EXPAND_FOR_ALL_TYPES

#undef EXPAND_TYPE

// Additional implicit conversion to double
DECLARE_OR_DEFINE_STD_CONVERSION(float, double);
DECLARE_OR_DEFINE_STD_CONVERSION(int32_t, double);
DECLARE_OR_DEFINE_STD_CONVERSION(int16_t, double);
DECLARE_OR_DEFINE_STD_CONVERSION(int8_t, double);

// Additional implicit conversion to float
DECLARE_OR_DEFINE_STD_CONVERSION(int16_t, float);
DECLARE_OR_DEFINE_STD_CONVERSION(int8_t, float);

// Additional implicit conversion to int64_t
DECLARE_OR_DEFINE_STD_CONVERSION(int32_t, int64_t);
DECLARE_OR_DEFINE_STD_CONVERSION(int16_t, int64_t);
DECLARE_OR_DEFINE_STD_CONVERSION(int8_t, int64_t);

// Additional implicit conversion to int32_t
DECLARE_OR_DEFINE_STD_CONVERSION(int16_t, int32_t);
DECLARE_OR_DEFINE_STD_CONVERSION(int8_t, int32_t);

#undef DECLARE_OR_DEFINE_STD_CONVERSION

} // namespace dbal

} // namespace madlib

#endif
