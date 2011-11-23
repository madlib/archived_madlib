/* ----------------------------------------------------------------------- *//**
 *
 * @file gamma.hpp
 *
 * @brief Evaluate the gamma distribution function using the boost library.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_PROB_GAMMA_H
#define MADLIB_PROB_GAMMA_H

#include <modules/common.hpp>

namespace madlib {
  namespace modules {
	namespace prob {

	  AnyType gamma_cdf(AbstractDBInterface &db, AnyType args);
	  AnyType gamma_pdf(AbstractDBInterface &db, AnyType args);
	  AnyType gamma_quantile(AbstractDBInterface &db, AnyType args);

	} // namespace prob
  } // namespace modules
} // namespace regress

#endif
