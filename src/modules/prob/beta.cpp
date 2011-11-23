/* ----------------------------------------------------------------------- *//** 
 *
 * @file beta.cpp
 *
 * @brief Evaluate the Beta distribution function using the boost library.
 *
 *//* -------------------------------------------------------------------- */

#include <modules/prob/beta.hpp>

#include <float.h>
#include <boost/math/distributions/beta.hpp>

namespace madlib {
  namespace modules {
	namespace prob {

	  /**
	   * @brief Beta cumulative distribution function: In-database interface
	   * 
	   * Note: Implemented using the boost library.
	   */
	  AnyType beta_cdf(AbstractDBInterface & /* db */, AnyType args) {
		AnyType::iterator   arg(args);
		const double        x     = *arg++;
		const double        alpha = *arg++;
		const double        beta  = *arg;
        
		/* We want to ensure nu > 0 */
		if (x < 0 || x > 1)
		  throw std::domain_error("beta distribution undefined for "
								  "x < 0 or x > 1");
		if (alpha <= 0)
		    throw std::domain_error("beta distribution undefined for "
									"alpha <= 0");
		if (beta <= 0)
		    throw std::domain_error("beta distribution undefined for "
									"beta <= 0");

		boost::math::beta_distribution<> dist(alpha, beta);		        
		return boost::math::cdf( dist, x);
	  }

	  /**
	   * @brief beta probability density function: In-database interface
	   * 
	   * Note: Implemented using the boost library.
	   */
	  AnyType beta_pdf(AbstractDBInterface & /* db */, AnyType args) {
		AnyType::iterator   arg(args);
		const double        x     = *arg++;
		const double        alpha = *arg++;
		const double        beta  = *arg;

		if (x < 0 || x > 1)
		  throw std::domain_error("beta distribution undefined for "
								  "x < 0 or x > 1");
		if (alpha <= 0)
		    throw std::domain_error("beta distribution undefined for "
									"alpha <= 0");
		if (beta <= 0)
		    throw std::domain_error("beta distribution undefined for "
									"beta <= 0");

		boost::math::beta_distribution<> dist(alpha, beta);		        
		return boost::math::pdf( dist, x);
	  }

	  /**
	   * @brief beta quantile function: In-database interface
	   * 
	   * Note: Implemented using the boost library.
	   */
	  AnyType beta_quantile(AbstractDBInterface & /* db */, AnyType args) {
		AnyType::iterator   arg(args);
		const double        p     = *arg++;
		const double        alpha = *arg++;
		const double        beta  = *arg;
        
		/* We want to ensure nu > 0 */
		if (p < 0 || p > 1)
		    throw std::domain_error("probability must be in the "
									"range [0,1]");
		if (alpha <= 0)
		    throw std::domain_error("beta distribution undefined for "
									"alpha <= 0");
		if (beta <= 0)
		    throw std::domain_error("beta distribution undefined for "
									"beta <= 0");

		boost::math::beta_distribution<> dist(alpha, beta);		
		return boost::math::quantile( dist, p);
	  }

	} // namespace prob
  } // namespace modules
} // namespace regress
