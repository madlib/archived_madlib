/* ----------------------------------------------------------------------- *//**
 *
 * @file linear.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_REGRESSION_H
#define MADLIB_REGRESSION_H

#include <madlib/modules/modules.hpp>

namespace madlib {

namespace modules {

namespace regress {

struct linearRegression {
    static AnyValue transition(AbstractDBInterface &db, AnyValue args);
    static AnyValue final(AbstractDBInterface &db, AnyValue args);
};

} // namespace regress

} // namespace modules

} // namespace regress

#endif
