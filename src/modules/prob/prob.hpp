/* -----------------------------------------------------------------------------
 *
 * @file prob.hpp
 *
 * @brief Umbrella header that includes all probability functions
 *
 * -------------------------------------------------------------------------- */

namespace madlib {

namespace modules {

/**
 * @brief Probability functions
 */
namespace prob {

#ifdef MADLIB_CURRENT_NAMESPACE
    #undef MADLIB_CURRENT_NAMESPACE
#endif

#define MADLIB_CURRENT_NAMESPACE madlib::modules::prob

#include "boost.hpp"
#include "student.hpp"

} // namespace prob

} // namespace modules

} // namespace madlib

