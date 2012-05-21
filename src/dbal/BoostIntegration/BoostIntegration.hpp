/* ----------------------------------------------------------------------- *//**
 *
 * @file BoostIntegration.hpp
 *
 * Implementing the Boost assertion handlers and user-defined functions is not
 * without issues.
 *
 * On system with a flat namespace (Linux), the main image might also have
 * definitions of the boost assertion handlers, which would be used (unless
 * the connector library is loaded with RTLD_LOCAL ORed in to the call to
 * dlopen).
 *
 * On systems with layered namespace (direct binding), the "one definition rule"
 * could be violated.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_DBAL_BOOST_INTEGRATION_HPP
#define MADLIB_DBAL_BOOST_INTEGRATION_HPP

#include "Assertions_impl.hpp"
#include "MathToolkit_impl.hpp"

#endif // !defined(MADLIB_BOOST_INTEGRATION_HPP)
