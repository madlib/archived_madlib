/* ----------------------------------------------------------------------- *//**
 *
 * @file dbal.hpp
 *
 * @brief Header file for database abstraction layer
 * @author Florian Schoppmann
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_HPP
// Note: We also use MADLIB_DBAL_HPP for a workaround for Doxygen:
// *_{impl|proto}.hpp files should be ingored if not included by
// EigenLinAlgTypes.hpp
#define MADLIB_DBAL_HPP

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

#include "OutputStreamBufferBase_proto.hpp"
#include "OutputStreamBufferBase_impl.hpp"
#include "Exceptions/NoSolutionFoundException_proto.hpp"

} // namespace dbal

} // namespace madlib

#endif // defined(MADLIB_DBAL_HPP)
