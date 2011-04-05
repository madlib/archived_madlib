/* ----------------------------------------------------------------------- *//**
 *
 * @file modules.hpp
 *
 * @brief Common header file all modules are supposed to include
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_HPP
#define MADLIB_MODULES_HPP

// Inlcude Database Abstraction Layer

#include <madlib/dbal/dbal.hpp>
#include <madlib/utils/memory.hpp>

// Import commonly used names into the modules namespace

namespace madlib {

namespace modules {

using namespace dbal;
using namespace utils::memory;

} // namespace modules

} // namespace madlib

#endif
