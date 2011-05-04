/* ----------------------------------------------------------------------- *//**
 *
 * @file postgres.hpp
 *
 * @brief Common header file for PostgreSQL port
 *
 * This file is included by all PostgreSQL-specific source files.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_HPP
#define MADLIB_POSTGRES_HPP

#include <madlib/dbal/dbal.hpp>
#include <madlib/utils/memory.hpp>

extern "C" {
    #include <postgres.h>   // for Oid, Datum
}

namespace madlib {

namespace ports {

namespace postgres {

using namespace dbal;
using namespace utils::memory;

typedef AnyValue (MADFunction)(AbstractDBInterface &, AnyValue);

// Forward declarations
// ====================

// Abstract Base Classes

class PGInterface;
class PGAllocator;

} // namespace postgres

} // namespace ports

} // namespace madlib

#endif
