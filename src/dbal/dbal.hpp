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

// Boost

// Handle failed Boost assertions in a sophisticated way. This is implemented
// in assert.cpp
#define BOOST_ENABLE_ASSERT_HANDLER

//   Boost provides definitions from C99 (which is not part of C++03)
#include <boost/cstdint.hpp>

//   Smart pointers
#include <boost/tr1/memory.hpp>
#include <boost/utility/enable_if.hpp>

//   Useful type traits
#include <boost/type_traits/is_same.hpp>
#include <boost/type_traits/is_const.hpp>

//   Array
#include <boost/multi_array.hpp>


//   Floating-point classification functions are in C99 and TR1, but not in the
//   official C++ Standard (before C++11). We therefore use the Boost
//   implementation. We need it, e.g., in dbal/EigenPlugin.hpp, which calls
//   isfinite().
#include <boost/math/special_functions/fpclassify.hpp>


// Utilities

#include <utils/memory.hpp>
#include <utils/shapeToExtents.hpp>
#include <utils/Reference.hpp>


// All data types for which ConcreteType<T> classes should be created and
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
    EXPAND_TYPE(ArmadilloTypes::DoubleCol) \
    EXPAND_TYPE(ArmadilloTypes::DoubleCol_const) \
    EXPAND_TYPE(ArmadilloTypes::DoubleMat) \
    EXPAND_TYPE(ArmadilloTypes::DoubleRow) \
    EXPAND_TYPE(ArmadilloTypes::DoubleRow_const) \
    EXPAND_TYPE(EigenTypes<Eigen::Unaligned>::DoubleCol) \
    EXPAND_TYPE(EigenTypes<Eigen::Unaligned>::DoubleCol_const) \
    EXPAND_TYPE(EigenTypes<Eigen::Unaligned>::DoubleMat) \
    EXPAND_TYPE(EigenTypes<Eigen::Unaligned>::DoubleMat_const) \
    EXPAND_TYPE(EigenTypes<Eigen::Unaligned>::DoubleRow) \
    EXPAND_TYPE(EigenTypes<Eigen::Unaligned>::DoubleRow_const) \
    EXPAND_TYPE(EigenTypes<Eigen::Aligned>::DoubleCol) \
    EXPAND_TYPE(EigenTypes<Eigen::Aligned>::DoubleCol_const) \
    EXPAND_TYPE(EigenTypes<Eigen::Aligned>::DoubleMat) \
    EXPAND_TYPE(EigenTypes<Eigen::Aligned>::DoubleMat_const) \
    EXPAND_TYPE(EigenTypes<Eigen::Aligned>::DoubleRow) \
    EXPAND_TYPE(EigenTypes<Eigen::Aligned>::DoubleRow_const) \
    EXPAND_TYPE(AnyTypeVector)


namespace madlib {

// Import names that are integral parts of MADlib

using std::tr1::shared_ptr;
using std::tr1::dynamic_pointer_cast;
//using boost::enable_if;
//using boost::disable_if;
using boost::is_same;
using boost::multi_array;
using boost::multi_array_ref;
using boost::const_multi_array_ref;

/**
 * @brief Database-abstraction-layer classes (independent of a particular DBMS)
 */
namespace dbal {

// Forward declarations
// ====================

// Abstract Base Classes

class AbstractAllocator;
class AbstractHandle;
class AbstractDBInterface;
class AbstractType;
class AbstractTypeConverter;

typedef shared_ptr<AbstractAllocator> AllocatorSPtr;
typedef shared_ptr<AbstractHandle> MemHandleSPtr;
typedef shared_ptr<const AbstractType> AbstractTypeSPtr;

// Type Classes

template <typename T, std::size_t NumDims = 1> class Array;
template <typename T, std::size_t NumDims = 1> class Array_const;

// Implementation Classes

class AnyType;
struct Null { };
template <typename T> class ConcreteType;

typedef std::vector<AnyType> AnyTypeVector;
typedef ConcreteType<AnyTypeVector> ConcreteRecord;


// Includes
// ========

// Abstract Base Clases (Headers only)

#include <dbal/AbstractAllocator.hpp>
#include <dbal/AbstractHandle.hpp>

} // namespace dbal

} // namespace madlib

// Matrix and Vector Types
#include <armadillo>

#if !defined(ARMA_USE_LAPACK) || !defined(ARMA_USE_BLAS)
    #error Armadillo has been configured without LAPACK and/or BLAS. Cannot continue.
#endif

#define EIGEN_MATRIXBASE_PLUGIN <dbal/EigenPlugin.hpp>
#include <Eigen/Dense>

namespace madlib {

namespace dbal {

#include <dbal/ArmadilloTypes.hpp>
#include <dbal/EigenTypes.hpp>
#include <dbal/AbstractOutputStreamBuffer.hpp>
#include <dbal/AbstractDBInterface.hpp>
#include <dbal/AbstractType_proto.hpp>
#include <dbal/AbstractTypeConverter.hpp>

// Type Classes

// Array depends on Array_const, so including Array_const first
#include <dbal/Array.hpp>
#include <dbal/Array_const.hpp>

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

#include <dbal/AnyType_proto.hpp>
#include <dbal/ConcreteType_proto.hpp>

// Implementation Classes

#include <dbal/AbstractType_impl.hpp>
#include <dbal/AnyType_impl.hpp>
#include <dbal/ConcreteType_impl.hpp>


} // namespace dbal


// Setup modules namespace
// =======================

namespace modules {

// Import commonly used names into the modules namespace

using namespace dbal;
using namespace utils::memory;

// Declaration Macros

#define LINEAR_ALGEBRA_POLICY_DEFINITIONS(Policy) \
    typedef typename Policy::DoubleMat DoubleMat; \
    typedef typename Policy::DoubleCol DoubleCol; \
    typedef typename Policy::DoubleCol_const DoubleCol_const; \
    typedef typename Policy::DoubleRow DoubleRow; \
    typedef typename Policy::DoubleRow_const DoubleRow_const

#define DECLARE_UDF_WITH_POLICY(name) \
    template <class Policy> \
    struct name : public Policy { \
        LINEAR_ALGEBRA_POLICY_DEFINITIONS(Policy); \
        static AnyType eval(AbstractDBInterface &db, AnyType args); \
    };

#define DECLARE_UDF(name) \
    AnyType name(AbstractDBInterface &db, AnyType args);


// Should DECLARE_UDF be within the namespace of the respective module?
#define MADLIB_DECLARE_NAMESPACES

} // namespace modules

} // namespace madlib


/**
 * @namespace madlib::dbconnector
 *
 * @brief Connector classes for specific DBMSs
 */

#endif
