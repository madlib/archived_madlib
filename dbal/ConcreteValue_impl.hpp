/* ----------------------------------------------------------------------- *//**
 *
 * @file ConcreteValue_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

template <typename T>
inline ConcreteValue<T>::ConcreteValue(const T &inValue)
: mValue(inValue), mIsNull(false) { }

template <typename T>
inline AbstractValueSPtr ConcreteValue<T>::getValueByID(unsigned int inID) const {
    using utils::memory::NoDeleter;

    if (inID != 0)
        throw std::out_of_range("Tried to access non-zero index of non-tuple type");

    return AbstractValueSPtr(this, NoDeleter<const AbstractValue>());
}


// Spezializations for ConcreteValue<AnyValueVector>

template <>
inline unsigned int ConcreteValue<AnyValueVector>::size() const {
    return mValue.size();
}

template <>
inline bool ConcreteValue<AnyValueVector>::isCompound() const {
    return true;
}

template <>
inline AbstractValueSPtr ConcreteValue<AnyValueVector>::getValueByID(
    unsigned int inID) const {

    using utils::memory::NoDeleter;
    
    if (inID >= mValue.size())
        throw std::out_of_range("Index out of bounds when accessing tuple");
    
    return AbstractValueSPtr( &mValue[inID], NoDeleter<const AbstractValue>() );
}


// Specializations to allow lossless imcplicit conversion

#define DECLARE_OR_DEFINE_STD_CONVERSION(x,y) \
    template <> \
    inline y ConcreteValue<x >::getAs(y* /* pure type parameter */) const { \
        return mValue; \
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

template <>
inline DoubleCol ConcreteValue<Array<double> >::getAs(DoubleCol*) const {
    return DoubleCol(
        TransparentHandle::create(const_cast<double*>(mValue.data())),
        mValue.size());
}

template <>
inline DoubleRow ConcreteValue<Array<double> >::getAs(DoubleRow*) const {
    return DoubleRow(
        TransparentHandle::create(const_cast<double*>(mValue.data())),
        mValue.size());
}
