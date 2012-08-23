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

/**
 * @brief Memory context to use for allocation (hint only)
 *
 * This is only meant to be a hint to the platform-specific parts of the AL.
 * An implementation may choose to perform an allocation in a specific aggregate
 * context if the memory needs to live longer than the current function call.
 * It should be used, e.g., when allocating transition states.
 */
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
// #include "EigenIntegration/EigenIntegration.hpp"
#include "Exceptions/NoSolutionFoundException_proto.hpp"

#include "ByteStream_proto.hpp"
#include "ByteStreamHandleBuf_proto.hpp"
#include "DynamicStruct_proto.hpp"
#include "OutputStreamBufferBase_proto.hpp"
#include "Reference_proto.hpp"

#endif // defined(MADLIB_DBAL_HPP)
