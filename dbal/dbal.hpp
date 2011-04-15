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

// STL dependencies

#include <vector>

// Other dependencies

// Array
#include <boost/multi_array.hpp>

// Matrix, Vector
#include <armadillo>

// All sources need to (implicitly or explicitly) include madlib.hpp, so we also
// include it here

#include <madlib/madlib.hpp>
#include <madlib/utils/memory.hpp>
#include <madlib/utils/shapeToExtents.hpp>

// All data types for which ConcreteValue<T> classes should be created and
// for which conversion functions have to be supplied

#define EXPAND_FOR_PRIMITIVE_TYPES \
    EXPAND_TYPE(double) \
    EXPAND_TYPE(float) \
    EXPAND_TYPE(int64_t) \
    EXPAND_TYPE(int32_t) \
    EXPAND_TYPE(int16_t) \
    EXPAND_TYPE(int8_t) \
    EXPAND_TYPE(bool)

#define EXPAND_FOR_ALL_TYPES \
    EXPAND_FOR_PRIMITIVE_TYPES \
    EXPAND_TYPE(Array<double>) \
    EXPAND_TYPE(Array_const<double>) \
    EXPAND_TYPE(DoubleCol) \
    EXPAND_TYPE(DoubleCol_const) \
    EXPAND_TYPE(DoubleMat) \
    EXPAND_TYPE(DoubleRow) \
    EXPAND_TYPE(DoubleRow_const) \
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
template <typename T, std::size_t NumDims = 1> class Array_const;
template <template <class> class T, typename eT> class Vector;
template <template <class> class T, typename eT> class Vector_const;
template <typename eT> class Matrix;

typedef Matrix<double> DoubleMat;
typedef Vector<arma::Col, double> DoubleCol;
typedef Vector_const<arma::Col, double> DoubleCol_const;
typedef Vector<arma::Row, double> DoubleRow;
typedef Vector_const<arma::Row, double> DoubleRow_const;

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

// Array depends on Array_const, so including Array_const first
#include <madlib/dbal/Array.hpp>
#include <madlib/dbal/Array_const.hpp>
#include <madlib/dbal/Matrix.hpp>
#include <madlib/dbal/Vector.hpp>
#include <madlib/dbal/Vector_const.hpp>

} // namespace dbal

} // namespace madlib


// Integration with Armadillo
// ==========================

namespace arma {

#include <madlib/dbal/ArmadilloIntegration.hpp>

}

// DBAL Implementation Includes
// ============================

namespace madlib {

namespace dbal {

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
