/* ----------------------------------------------------------------------- *//**
 *
 * @file madlib.hpp
 *
 * @brief Common header file with global macros etc.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_HPP
#define MADLIB_HPP

#include <boost/tr1/memory.hpp>
#include <boost/utility/enable_if.hpp>

namespace madlib {

using std::tr1::shared_ptr;
using std::tr1::dynamic_pointer_cast;
using boost::enable_if;
using boost::disable_if;

} // namespace madlib

#endif
