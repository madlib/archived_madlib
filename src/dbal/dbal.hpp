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

#include <locale>
#include <vector>

// Other dependencies

// Array
#include <boost/multi_array.hpp>

// Matrix, Vector
#include <armadillo>

#if !defined(ARMA_USE_LAPACK) || !defined(ARMA_USE_BLAS)
    #error Armadillo has been configured without LAPACK and/or BLAS. Cannot continue.
#endif

// All sources need to (implicitly or explicitly) include madlib.hpp, so we also
// include it here

#include <madlib.hpp>
#include <utils/memory.hpp>
#include <utils/shapeToExtents.hpp>

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

#include <dbal/AbstractAllocator.hpp>
#include <dbal/AbstractHandle.hpp>
#include <dbal/AbstractOutputStreamBuffer.hpp>
#include <dbal/AbstractDBInterface.hpp>
#include <dbal/AbstractValue_proto.hpp>
#include <dbal/AbstractValueConverter.hpp>

// Type Classes

// Array depends on Array_const, so including Array_const first
#include <dbal/Array.hpp>
#include <dbal/Array_const.hpp>
#include <dbal/Matrix.hpp>
#include <dbal/Vector.hpp>
#include <dbal/Vector_const.hpp>

} // namespace dbal

} // namespace madlib


// Integration with Armadillo
// ==========================

namespace arma {

#include <dbal/ArmadilloIntegration.hpp>

}

// DBAL Implementation Includes
// ============================

namespace madlib {

namespace dbal {

// Simple Helper Classes

#include <dbal/TransparentHandle.hpp>

// Implementation Classes (Headers)

#include <dbal/AnyValue_proto.hpp>
#include <dbal/ConcreteValue_proto.hpp>
#include <dbal/ValueConverter_proto.hpp>

// Implementation Classes

#include <dbal/AbstractValue_impl.hpp>
#include <dbal/AnyValue_impl.hpp>
#include <dbal/ConcreteValue_impl.hpp>
#include <dbal/ValueConverter_impl.hpp>


} // namespace dbal

} // namespace madlib

#endif
