/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractValue_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#define EXPAND_TYPE(T) \
    T AbstractValue::getAs(T* /* pure type parameter */) const { \
        throw std::logic_error("Internal error: Unsupported type conversion requested"); \
    }

EXPAND_FOR_ALL_TYPES

#undef EXPAND_TYPE
