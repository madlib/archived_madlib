/* ----------------------------------------------------------------------- *//**
 *
 * @file main.cpp
 *
 * @brief Main file containing the entrance points into the C++ modules
 *
 *//* ----------------------------------------------------------------------- */

// We do not write #include "dbconnector.hpp" here because we want to rely on
// the search paths, which might point to a port-specific dbconnector.hpp
#include <dbconnector/dbconnector.hpp>
#include <utils/MallocAllocator.hpp>

extern "C" {
    PG_MODULE_MAGIC;
} // extern "C"

namespace madlib {

namespace dbconnector {

namespace postgres {

bool AnyType::sLazyConversionToDatum = false;

namespace {

// No need to export these names to other translation units.
#ifndef NDEBUG
OutputStreamBuffer<INFO, utils::MallocAllocator> gOutStreamBuffer;
OutputStreamBuffer<WARNING, utils::MallocAllocator> gErrStreamBuffer;
#endif

}

#ifndef NDEBUG
/**
 * @brief Informational output stream
 */
std::ostream dbout(&gOutStreamBuffer);

/**
 * @brief Warning and non-fatal error output stream
 */
std::ostream dberr(&gErrStreamBuffer);
#endif
} // namespace postgres

} // namespace dbconnector

} // namespace madlib

// Include declarations declarations
#include <modules/declarations.hpp>

// Now export the symbols
#undef DECLARE_UDF
#undef DECLARE_SR_UDF
#define DECLARE_UDF DECLARE_UDF_EXTERNAL
#define DECLARE_SR_UDF DECLARE_UDF_EXTERNAL
#include <modules/declarations.hpp>
