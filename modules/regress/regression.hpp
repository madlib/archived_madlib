/*-------------------------------------------------------------------------
 *
 * @file regression.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_REGRESSION_H
#define MADLIB_REGRESSION_H

#include <madlib/modules/modules.hpp>

namespace madlib {

namespace modules {

namespace regress {

struct multiply {
    AnyValue operator()(AnyValue::iterator arg) const;
};

struct add {
    AnyValue operator()(AnyValue::iterator arg) const;
};

struct subtract {
    AnyValue operator()(AnyValue::iterator arg) const;
};

} // namespace regress

} // namespace modules

} // namespace regress

#endif
