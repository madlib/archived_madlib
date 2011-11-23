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

AnyType chi_squared_cdf(AbstractDBInterface &db, AnyType args);
AnyType chi_squared_pdf(AbstractDBInterface &db, AnyType args);
AnyType chi_squared_quantile(AbstractDBInterface &db, AnyType args);

} // namespace prob

} // namespace modules

} // namespace regress

#endif
