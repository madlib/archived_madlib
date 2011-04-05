/* ----------------------------------------------------------------------- *//**
 *
 * @file madlib.hpp
 *
 * @brief Common header file with global macros etc.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_HPP
#define MADLIB_HPP

#include <boost/cstdint.hpp>
#include <boost/tr1/memory.hpp>
#include <boost/utility/enable_if.hpp>
#include <boost/type_traits/is_same.hpp>
#include <boost/multi_array.hpp>

namespace madlib {

using std::tr1::shared_ptr;
using std::tr1::dynamic_pointer_cast;
using boost::enable_if;
using boost::disable_if;
using boost::is_same;
using boost::multi_array;
using boost::multi_array_ref;
using boost::const_multi_array_ref;

} // namespace madlib

#endif
