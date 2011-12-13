/* ----------------------------------------------------------------------- *//**
 *
 * @file dbal.hpp
 *
 * @brief Header file for database abstraction layer
 * @author Florian Schoppmann
 *
 *//* ----------------------------------------------------------------------- */

namespace madlib {

/**
 * @brief Database-abstraction layer
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

#include "OutputStreamBase.hpp"

}

}
