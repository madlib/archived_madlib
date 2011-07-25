/* ----------------------------------------------------------------------- *//**
 *
 * @file AbstractType_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#define EXPAND_TYPE(T) \
    T AbstractType::getAs(T* /* pure type parameter */) const { \
        throw std::logic_error("Internal error: Unsupported type conversion requested"); \
    }

EXPAND_FOR_ALL_TYPES

#undef EXPAND_TYPE

//AnyType AbstractType::clone() const {
//    return AnyType();
//}

//AnyType AbstractType::getValueByID(uint16_t inID) const {
//    return AnyType();
//}
