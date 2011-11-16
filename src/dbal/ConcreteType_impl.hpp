/* ----------------------------------------------------------------------- *//**
 *
 * @file ConcreteType_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

template <typename T>
inline ConcreteType<T>::ConcreteType(const T &inValue)
: mValue(inValue) { }

template <typename T>
inline AbstractTypeSPtr ConcreteType<T>::getValueByID(uint16_t inID) const {
    if (inID > 0)
        throw std::out_of_range("Index out of bounds while accessing "
            "non-composite value");
    
    return AbstractTypeSPtr(new ConcreteType<T>(*this));
}


// Spezializations for ConcreteType<AnyTypeVector>

template <>
inline unsigned int ConcreteType<AnyTypeVector>::size() const {
    return mValue.size();
}

template <>
inline bool ConcreteType<AnyTypeVector>::isCompound() const {
    return true;
}

template <>
inline AbstractTypeSPtr ConcreteType<AnyTypeVector>::getValueByID(
    uint16_t inID) const {

    if (inID >= mValue.size())
        throw std::out_of_range("Index out of bounds while accessing "
            "composite value");
    
    return mValue[inID].mDelegate;
}


// Specializations to allow lossless imcplicit conversion

#define DECLARE_OR_DEFINE_STD_CONVERSION(x,y) \
    template <> \
    inline y ConcreteType<x >::getAs(y* /* pure type parameter */) const { \
        return mValue; \
    }

#define EXPAND_TYPE(T) \
    DECLARE_OR_DEFINE_STD_CONVERSION(T, T)

EXPAND_FOR_ALL_TYPES

#undef EXPAND_TYPE

// Additional implicit conversion to double
DECLARE_OR_DEFINE_STD_CONVERSION(float, double)
DECLARE_OR_DEFINE_STD_CONVERSION(int32_t, double)
DECLARE_OR_DEFINE_STD_CONVERSION(int16_t, double)
DECLARE_OR_DEFINE_STD_CONVERSION(int8_t, double)
DECLARE_OR_DEFINE_STD_CONVERSION(bool, double)

// Additional implicit conversion to float
DECLARE_OR_DEFINE_STD_CONVERSION(int16_t, float)
DECLARE_OR_DEFINE_STD_CONVERSION(int8_t, float)
DECLARE_OR_DEFINE_STD_CONVERSION(bool, float)

// Additional implicit conversion to int64_t
DECLARE_OR_DEFINE_STD_CONVERSION(int32_t, int64_t)
DECLARE_OR_DEFINE_STD_CONVERSION(int16_t, int64_t)
DECLARE_OR_DEFINE_STD_CONVERSION(int8_t, int64_t)
DECLARE_OR_DEFINE_STD_CONVERSION(bool, int64_t)

// Additional implicit conversion to int32_t
DECLARE_OR_DEFINE_STD_CONVERSION(int16_t, int32_t)
DECLARE_OR_DEFINE_STD_CONVERSION(int8_t, int32_t)
DECLARE_OR_DEFINE_STD_CONVERSION(bool, int32_t)

// Additional implicit conversion to int16_t
DECLARE_OR_DEFINE_STD_CONVERSION(int8_t, int16_t)
DECLARE_OR_DEFINE_STD_CONVERSION(bool, int16_t)

// Additional implicit conversion to int8_t
DECLARE_OR_DEFINE_STD_CONVERSION(bool, int8_t)

// Additional implicit conversion from Array<double>
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, Array_const<double>)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, ArmadilloTypes::DoubleCol)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, ArmadilloTypes::DoubleCol_const)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, ArmadilloTypes::DoubleRow)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, ArmadilloTypes::DoubleRow_const)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, EigenTypes<Eigen::Unaligned>::DoubleCol)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, EigenTypes<Eigen::Unaligned>::DoubleCol_const)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, EigenTypes<Eigen::Unaligned>::DoubleRow)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, EigenTypes<Eigen::Unaligned>::DoubleRow_const)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, EigenTypes<Eigen::Aligned>::DoubleCol)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, EigenTypes<Eigen::Aligned>::DoubleCol_const)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, EigenTypes<Eigen::Aligned>::DoubleRow)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, EigenTypes<Eigen::Aligned>::DoubleRow_const)

// Additional implicit conversion from Array_const<double>
DECLARE_OR_DEFINE_STD_CONVERSION(Array_const<double>, ArmadilloTypes::DoubleCol_const)
DECLARE_OR_DEFINE_STD_CONVERSION(Array_const<double>, ArmadilloTypes::DoubleRow_const)
DECLARE_OR_DEFINE_STD_CONVERSION(Array_const<double>, EigenTypes<Eigen::Unaligned>::DoubleCol_const)
DECLARE_OR_DEFINE_STD_CONVERSION(Array_const<double>, EigenTypes<Eigen::Unaligned>::DoubleRow_const)
DECLARE_OR_DEFINE_STD_CONVERSION(Array_const<double>, EigenTypes<Eigen::Aligned>::DoubleCol_const)
DECLARE_OR_DEFINE_STD_CONVERSION(Array_const<double>, EigenTypes<Eigen::Aligned>::DoubleRow_const)

// FIXME: We might want to add detailed error messages when converting immutable
// into mutable type. Right now, the standard error msg will be displayed.

#undef DECLARE_OR_DEFINE_STD_CONVERSION

template <>
inline bool ConcreteType<Array_const<double> >::isMutable() const {
    return false;
}

template <>
inline AbstractTypeSPtr ConcreteType<Array_const<double> >::clone() const {
    return
        AbstractTypeSPtr(
            new ConcreteType<Array<double> >(
                Array<double>(
                    mValue.memoryHandle()->clone(), 
                    utils::shapeToExtents<1>(mValue.shape())
                )
            )
        );
}
