/* ----------------------------------------------------------------------- *//**
 *
 * @file ValueConverter.cpp
 *
 *//* ----------------------------------------------------------------------- */

#include <madlib/dbal/ValueConverter.hpp>

namespace madlib {

namespace dbal {

#ifdef DECLARE_OR_DEFINE_STD_CONVERSION
    #error "DECLARE_OR_DEFINE_STD_CONVERSION was declared before."
#endif
#define DECLARE_OR_DEFINE_STD_CONVERSION(x,y) \
    template <> \
    void ValueConverter<y>::convert(const x &inValue) { \
        mConvertedValue = inValue; \
    }

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
