/* -----------------------------------------------------------------------------
 *
 * @file regress.hpp
 *
 * @brief Umbrella header that includes all linear/logistic regression headers
 *
 * -------------------------------------------------------------------------- */

namespace madlib {

namespace modules {

/**
 * @brief Linear/logistic regression functions
 */
namespace regress {

#ifdef MADLIB_CURRENT_NAMESPACE
    #undef MADLIB_CURRENT_NAMESPACE
#endif

#define MADLIB_CURRENT_NAMESPACE madlib::modules::regress

#include "linear.hpp"
#include "logistic.hpp"

} // namespace regress

} // namespace modules

} // namespace madlib
