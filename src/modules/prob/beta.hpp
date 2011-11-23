/* ----------------------------------------------------------------------- *//**
 *
 * @file beta.hpp
 *
 * @brief Evaluate the gamma distribution function using the boost library.
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_PROB_BETA_H
#define MADLIB_PROB_BETA_H

#include <modules/common.hpp>

namespace madlib {
  namespace modules {
	namespace prob {

	  AnyType beta_cdf(AbstractDBInterface &db, AnyType args);
	  AnyType beta_pdf(AbstractDBInterface &db, AnyType args);
	  AnyType beta_quantile(AbstractDBInterface &db, AnyType args);

	} // namespace prob
  } // namespace modules
} // namespace regress

#endif
