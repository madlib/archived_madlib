/* ----------------------------------------------------------------------- *//**
 *
 * @file modules.hpp
 *
 * @brief Common header file all modules are supposed to include
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_HPP
#define MADLIB_MODULES_HPP

// All sources need to (implicitly or explicitly) include madlib.hpp, so we also
// include it here

#include <madlib/madlib.hpp>


// Database Abstraction Layer

#include <madlib/dbal/AnyValue.hpp>
#include <madlib/dbal/AnyValue.hpp>
#include <madlib/dbal/ConcreteValue.hpp>
#include <madlib/dbal/Null.hpp>
#include <madlib/dbal/ValueConverter.hpp>


// Import commonly used names into the modules namespace

namespace madlib {

namespace modules {

using namespace dbal;

} // namespace modules

} // namespace madlib

#endif
