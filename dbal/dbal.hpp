/* ----------------------------------------------------------------------- *//**
 *
 * @file dbal.hpp
 *
 * @brief Header file for database abstraction layer
 * @author Florian Schoppmann
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_HPP
#define MADLIB_DBAL_HPP

// All sources need to (implicitly or explicitly) include madlib.hpp, so we also
// include it here

#include <madlib/madlib.hpp>
#include <madlib/utils/memory.hpp>

// STL dependencies

#include <vector>

// Other dependencies

// Array
#include <boost/multi_array.hpp>

// Matrix, Vector
#include <armadillo>


// All data types for which ConcreteValue<T> classes should be created and
// for which conversion functions have to be supplied

#define EXPAND_FOR_PRIMITIVE_TYPES \
    EXPAND_TYPE(double) \
    EXPAND_TYPE(float) \
    EXPAND_TYPE(int64_t) \
    EXPAND_TYPE(int32_t) \
    EXPAND_TYPE(int16_t) \
    EXPAND_TYPE(int8_t) \

#define EXPAND_FOR_ALL_TYPES \
    EXPAND_FOR_PRIMITIVE_TYPES \
    EXPAND_TYPE(Array<double>) \
    EXPAND_TYPE(DoubleCol) \
    EXPAND_TYPE(DoubleMat) \
    EXPAND_TYPE(DoubleRow) \
    EXPAND_TYPE(AnyValueVector)

namespace madlib {

namespace dbal {

// Forward declarations
// ====================

// Abstract Base Classes

class AbstractAllocator;
class AbstractHandle;
class AbstractDBInterface;
class AbstractValue;
class AbstractValueConverter;

typedef shared_ptr<AbstractAllocator> AllocatorSPtr;
typedef shared_ptr<AbstractHandle> MemHandleSPtr;
typedef shared_ptr<const AbstractValue> AbstractValueSPtr;

// Type Classes

template <typename T, std::size_t NumDims = 1> class Array;
template <template <class> class T, typename eT> class Vector;
template <typename eT> class Matrix;

typedef Matrix<double> DoubleMat;
typedef Vector<arma::Col, double> DoubleCol;
typedef Vector<arma::Row, double> DoubleRow;

// Implementation Classes

class AnyValue;
struct Null { };
template <typename T> class ConcreteValue;

typedef std::vector<AnyValue> AnyValueVector;
typedef ConcreteValue<AnyValueVector> ConcreteRecord;


// Includes
// ========

// Abstract Base Clases (Headers only)

#include <madlib/dbal/AbstractAllocator.hpp>
#include <madlib/dbal/AbstractHandle.hpp>
#include <madlib/dbal/AbstractDBInterface.hpp>
#include <madlib/dbal/AbstractValue_proto.hpp>
#include <madlib/dbal/AbstractValueConverter.hpp>

// Type Classes

#include <madlib/dbal/Array.hpp>
#include <madlib/dbal/Matrix.hpp>
#include <madlib/dbal/Vector.hpp>

// Simple Helper Classes

#include <madlib/dbal/TransparentHandle.hpp>

// Implementation Classes (Headers)

#include <madlib/dbal/AnyValue_proto.hpp>
#include <madlib/dbal/ConcreteValue_proto.hpp>
#include <madlib/dbal/ValueConverter_proto.hpp>

// Implementation Classes

#include <madlib/dbal/AbstractValue_impl.hpp>
#include <madlib/dbal/AnyValue_impl.hpp>
#include <madlib/dbal/ConcreteValue_impl.hpp>
#include <madlib/dbal/ValueConverter_impl.hpp>


} // namespace dbal

} // namespace madlib

#endif
