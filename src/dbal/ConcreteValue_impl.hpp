/* ----------------------------------------------------------------------- *//**
 *
 * @file ConcreteValue_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

template <typename T>
inline ConcreteType<T>::ConcreteType(const T &inValue)
: mValue(inValue) { }

template <typename T>
inline const AnyType &ConcreteType<T>::getValueByID(uint16_t inID) const {
    throw std::logic_error("getValueByID() should never be called for ConcreteType<T>.");
}


// Spezializations for ConcreteValue<AnyTypeVector>

template <>
inline unsigned int ConcreteType<AnyTypeVector>::size() const {
    return mValue.size();
}

template <>
inline bool ConcreteType<AnyTypeVector>::isCompound() const {
    return true;
}

template <>
inline const AnyType &ConcreteType<AnyTypeVector>::getValueByID(
    uint16_t inID) const {

    if (inID >= mValue.size())
        throw std::out_of_range("Index out of bounds when accessing tuple");
    
    return mValue[inID];
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
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, DoubleCol)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, DoubleCol_const)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, DoubleRow)
DECLARE_OR_DEFINE_STD_CONVERSION(Array<double>, DoubleRow_const)

// Additional implicit conversion from Array_const<double>
DECLARE_OR_DEFINE_STD_CONVERSION(Array_const<double>, DoubleCol_const)
DECLARE_OR_DEFINE_STD_CONVERSION(Array_const<double>, DoubleRow_const)

// FIXME: We might want to add detailed error messages when converting immutable
// into mutable type. Right now, the standard error msg will be displayed.

#undef DECLARE_OR_DEFINE_STD_CONVERSION

/*
// Conversion of mutable arrays to immutable arrays and vectors (no copying
// needed)
template <>
inline DoubleCol ConcreteValue<Array<double> >::getAs(DoubleCol*) const {
    return DoubleCol(mValue.memoryHandle(), mValue.size());
}

template <>
inline DoubleCol_const ConcreteValue<Array<double> >::getAs(DoubleCol_const*) const {
    return DoubleCol_const(
        TransparentHandle::create(const_cast<double*>(mValue.data())),
        mValue.size());
}

template <>
inline DoubleRow ConcreteValue<Array<double> >::getAs(DoubleRow*) const {
    return DoubleRow(
        TransparentHandle::create(const_cast<double*>(mValue.data())),
        mValue.size());
}

template <>
inline DoubleRow_const ConcreteValue<Array<double> >::getAs(DoubleRow_const*) const {
    return DoubleRow_const(
        TransparentHandle::create(const_cast<double*>(mValue.data())),
        mValue.size());
}

// Conversion of immutable arrays to mutable arrays and vectors (copying needed)
*/

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
