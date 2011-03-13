/* ----------------------------------------------------------------------- *//**
 *
 * @file dbal.hpp
 *
 * @brief Common header file for database abstraction layer
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_HPP
#define MADLIB_DBAL_HPP

// All sources need to (implicitly or explicitly) include madlib.hpp, so we also
// include it here

#include <madlib/madlib.hpp>

// Includes for type definitions

#include <vector>

// All data types for which ConcreteValue<T> classes should be created and
// for which conversion functions have to be supplied

#define EXPAND_FOR_ALL_TYPES \
    EXPAND_TYPE(double) \
    EXPAND_TYPE(float) \
    EXPAND_TYPE(int64_t) \
    EXPAND_TYPE(int32_t) \
    EXPAND_TYPE(int16_t) \
    EXPAND_TYPE(int8_t) \
    EXPAND_TYPE(AnyValueVector)

namespace madlib {

namespace dbal {

// Forward type declarations

class AbstractValue;
class AnyValue;
class AbstractValueConverter;
template <typename T> class ConcreteValue;

typedef shared_ptr<const AbstractValue> AbstractValueSPtr;
typedef std::vector<AnyValue> AnyValueVector;
typedef ConcreteValue<AnyValueVector> ConcreteRecord;

} // namespace dbal

} // namespace madlib

#endif
