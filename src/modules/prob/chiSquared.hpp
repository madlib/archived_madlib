/* ----------------------------------------------------------------------- *//**
 *
 * @file chiSquared.hpp
 *
 * @brief Evaluate the chi-squared distribution function using the boost library.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_PROB_CHISQUARED_H
#define MADLIB_PROB_CHISQUARED_H

#include <modules/common.hpp>

namespace madlib {

namespace modules {

namespace prob {

AnyValue chi_squared_cdf(AbstractDBInterface &db, AnyValue args);

} // namespace prob

} // namespace modules

} // namespace regress

#endif
