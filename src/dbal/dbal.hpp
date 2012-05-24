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

#define MADLIB_DEFAULT_EXCEPTION std::runtime_error("Internal error")
#define madlib_assert(_cond, _exception) \
    do { \
        if(!(_cond)) \
            throw _exception; \
    } while(false)

namespace madlib {

/**
 * @brief Database-independent abstractions
 */
namespace dbal {

enum TypeClass {
    SimpleType,
    ArrayType,
    CompositeType
};

enum Mutability {
    Immutable = 0,
    Mutable
};

enum MemoryContext {
    FunctionContext,
    AggregateContext
};

enum ZeroMemory {
    DoZero,
    DoNotZero
};

enum OnMemoryAllocationFailure {
    ReturnNULL,
    ThrowBadAlloc
};

} // namespace dbal

} // namespace madlib

// Boost integration is non-optional
#include "BoostIntegration/BoostIntegration.hpp"

#include "OutputStreamBufferBase_proto.hpp"
#include "OutputStreamBufferBase_impl.hpp"
#include "Exceptions/NoSolutionFoundException_proto.hpp"

#endif // defined(MADLIB_DBAL_HPP)
