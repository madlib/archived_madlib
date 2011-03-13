/* ----------------------------------------------------------------------- *//**
 *
 * @file ConcreteValue.hpp
 *
 * @brief Header file for concrete values
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_CONCRETEVALUE_HPP
#define MADLIB_CONCRETEVALUE_HPP

#include <madlib/dbal/dbal.hpp>
#include <madlib/dbal/AbstractValue.hpp>
#include <madlib/dbal/AbstractValueConverter.hpp>
#include <madlib/dbal/AnyValue.hpp>
#include <madlib/utils/memory.hpp>

#include <stdexcept>


namespace madlib {

namespace dbal {

using utils::memory::NoDeleter;


template <typename T>
class ConcreteValue : public AbstractValue {
    friend class AnyValue;

public:
    ConcreteValue() : mIsNull(true) { }
    ConcreteValue(const T &inValue) : mValue(inValue), mIsNull(false) { }

    void convert(AbstractValueConverter &inConverter) const {
        inConverter.convert(mValue);
    }

    const T &get() const {
        return mValue;
    }

protected:
    AbstractValueSPtr getValueByID(unsigned int inID) const {
        if (inID != 0)
            throw std::out_of_range("Tried to access non-zero index of non-tuple type");
    
        return AbstractValueSPtr(this, NoDeleter<const AbstractValue>());
    }

    AbstractValueSPtr clone() const {
        return AbstractValueSPtr( new ConcreteValue<T>(*this) );
    }

protected:
    const T mValue;
    const bool mIsNull;
};

template <>
class ConcreteValue<AnyValueVector> : public AbstractValue {
    friend class AnyValue;

public:
    typedef std::back_insert_iterator<AnyValueVector> iterator;

    ConcreteValue() : mIsNull(true) { }
    ConcreteValue(const AnyValueVector &inValue) : mValue(inValue), mIsNull(false) { }
    
    unsigned int size() const {
        return mValue.size();
    }

    bool isCompound() const {
        return true;
    }    

    void convert(AbstractValueConverter &inConverter) const {
        inConverter.convert(mValue);
    }
     
    const AnyValueVector &get() const {
        return mValue;
    }

protected:
    AbstractValueSPtr getValueByID(unsigned int inID) const {
        if (inID >= mValue.size())
            throw std::out_of_range("Index out of bounds when accessing tuple");
        
        return AbstractValueSPtr( &mValue[inID], NoDeleter<const AbstractValue>() );
    }

    AbstractValueSPtr clone() const {
        return AbstractValueSPtr( new ConcreteValue<AnyValueVector>(*this) );
    }

protected:
    const AnyValueVector mValue;
    const bool mIsNull;
};

} // namespace dbal

} // namespace madlib

#endif
