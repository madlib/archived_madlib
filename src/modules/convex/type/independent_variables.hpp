/* ----------------------------------------------------------------------- *//**
 *
 * @file independent_variables.hpp
 *
 * Types of independent variables can be simply vectors/matrices that are
 * offered by standard or third party libraries. Classes in this files are
 * for some special ones.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TYPE_INDEPENDENT_VARIABLES_HPP_
#define MADLIB_MODULES_CONVEX_TYPE_INDEPENDENT_VARIABLES_HPP_

namespace madlib {

namespace modules {

namespace convex {

struct MatrixIndex {
    int32_t i;
    int32_t j;
};

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

