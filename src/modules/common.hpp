/* ----------------------------------------------------------------------- *//**
 *
 * @file common.hpp
 *
 * @brief Common header file all modules are supposed to include
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_COMMON_HPP
#define MADLIB_MODULES_COMMON_HPP

// Handle failed Boost assertions in a sophisticated way. This is implemented
// in assert.cpp
#define BOOST_ENABLE_ASSERT_HANDLER

// Inlcude Database Abstraction Layer

#include <dbal/dbal.hpp>
#include <utils/memory.hpp>

// Import commonly used names into the modules namespace

namespace madlib {

namespace modules {

using namespace dbal;
using namespace utils::memory;

} // namespace modules

} // namespace madlib

#endif
