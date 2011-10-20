/* -----------------------------------------------------------------------------
 *
 * @file prob.hpp
 *
 * -------------------------------------------------------------------------- */

#ifdef MADLIB_DECLARE_NAMESPACES
namespace madlib {

namespace modules {

/**
 * @brief Probability functions
 */
namespace prob {
#endif

#ifdef MADLIB_CURRENT_NAMESPACE
#undef MADLIB_CURRENT_NAMESPACE
#endif

#define MADLIB_CURRENT_NAMESPACE madlib::modules::prob


#include <modules/prob/chiSquared.hpp>
#include <modules/prob/student.hpp>


#ifdef MADLIB_DECLARE_NAMESPACES
} // namespace prob

} // namespace modules

} // namespace madlib
#endif
