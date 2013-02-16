/* ----------------------------------------------------------------------- *//**
 * 
 * @file hessian.hpp
 *
 * This file contians classes of Hessian, which usually has
 * fields that maps to transition states for user-defined aggregates.
 * The necessity of these wrappers is to allow classes in algo/ and task/ to
 * have a type that they can template over.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_MODULES_CONVEX_TYPE_HESSIAN_HPP_
#define MADLIB_MODULES_CONVEX_TYPE_HESSIAN_HPP_

#include <modules/shared/HandleTraits.hpp>

namespace madlib {

namespace modules {

namespace convex {

typedef
    HandleTraits<MutableArrayHandle<double> >::MatrixTransparentHandleMap
    GLMHessian;

} // namespace convex

} // namespace modules

} // namespace madlib

#endif

