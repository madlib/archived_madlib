/* -----------------------------------------------------------------------------
 *
 * @file regress.hpp
 *
 * @brief Umbrella header that includes all linear/logistic regression headers
 *
 * -------------------------------------------------------------------------- */

//#ifndef MADLIB_DONT_DECLARE_NAMESPACES
    namespace madlib {

    namespace modules {

    /**
     * @brief Linear/logistic regression functions
     */
    namespace regress {
//#endif

#ifdef MADLIB_CURRENT_NAMESPACE
    #undef MADLIB_CURRENT_NAMESPACE
#endif

#define MADLIB_CURRENT_NAMESPACE madlib::modules::regress

#include <modules/regress/linear.hpp>

//#ifndef MADLIB_DONT_DECLARE_NAMESPACES
    } // namespace regress

    } // namespace modules

    } // namespace madlib
//#endif
