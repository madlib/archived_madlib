/* ----------------------------------------------------------------------- *//** 
 *
 * @file gamma.cpp
 *
 * @brief Evaluate the Gamma distribution function using the boost library.
 *
 *//* -------------------------------------------------------------------- */

#include <modules/prob/gamma.hpp>

#include <float.h>
#include <boost/math/distributions/gamma.hpp>

namespace madlib {
  namespace modules {
	namespace prob {

	  /**
	   * @brief gamma cumulative distribution function: In-database interface
	   * 
	   * Note: Implemented using the boost library.
	   */
	  AnyType gamma_cdf(AbstractDBInterface & /* db */, AnyType args) {
		AnyType::iterator   arg(args);
		const double        x     = *arg++;
		const double        shape = *arg++;
		const double        scale = *arg;
        
		/* We want to ensure nu > 0 */
		if (x < 0)
		    throw std::domain_error("gamma distribution undefined for "
									"random variate < 0");
		if (shape <= 0)
		    throw std::domain_error("gamma distribution undefined for "
									"shape <= 0");
		if (scale <= 0)
		    throw std::domain_error("gamma distribution undefined for "
									"scale <= 0");

		boost::math::gamma_distribution<> dist(shape, scale);		        
		return boost::math::cdf( dist, x);
	  }

	  /**
	   * @brief gamma probability density function: In-database interface
	   * 
	   * Note: Implemented using the boost library.
	   */
	  AnyType gamma_pdf(AbstractDBInterface & /* db */, AnyType args) {
		AnyType::iterator   arg(args);
		const double        x     = *arg++;
		const double        shape = *arg++;
		const double        scale = *arg;

		if (x < 0)
		    throw std::domain_error("gamma distribution undefined for "
									"random variate < 0");
		if (shape <= 0)
		    throw std::domain_error("gamma distribution undefined for "
									"shape <= 0");
		if (scale <= 0)
		    throw std::domain_error("gamma distribution undefined for "
									"scale <= 0");

		boost::math::gamma_distribution<> dist(shape, scale);		        
		return boost::math::pdf( dist, x);
	  }

	  /**
	   * @brief gamma quantile function: In-database interface
	   * 
	   * Note: Implemented using the boost library.
	   */
	  AnyType gamma_quantile(AbstractDBInterface & /* db */, AnyType args) {
		AnyType::iterator   arg(args);
		const double        p     = *arg++;
		const double        shape = *arg++;
		const double        scale = *arg;
        
		/* We want to ensure nu > 0 */
		if (p < 0 || p > 1)
		    throw std::domain_error("probability must be in the "
									"range [0,1]");
		if (shape <= 0)
		    throw std::domain_error("gamma distribution undefined for "
									"shape <= 0");
		if (scale <= 0)
		    throw std::domain_error("gamma distribution undefined for "
									"scale <= 0");
		if (p > 1 - DBL_EPSILON)
		    return INFINITY;

		boost::math::gamma_distribution<> dist(shape, scale);		
		return boost::math::quantile( dist, p);
	  }

	} // namespace prob
  } // namespace modules
} // namespace regress
