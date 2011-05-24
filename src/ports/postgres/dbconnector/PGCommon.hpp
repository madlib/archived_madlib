/* ----------------------------------------------------------------------- *//**
 *
 * @file PGCommon.hpp
 *
 * @brief Common header file for PostgreSQL port
 *
 * This file is included by all PostgreSQL-specific source files.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_COMMON_HPP
#define MADLIB_POSTGRES_COMMON_HPP

#include <dbal/dbal.hpp>
#include <utils/memory.hpp>

extern "C" {
    #include <postgres.h>   // for Oid, Datum
}

namespace madlib {

namespace dbconnector {

using namespace dbal;
using namespace utils::memory;

typedef AnyValue (MADFunction)(AbstractDBInterface &, AnyValue);

// Forward declarations
// ====================

// Abstract Base Classes

class PGInterface;
class PGAllocator;

} // namespace dbconnector

} // namespace madlib

#endif
