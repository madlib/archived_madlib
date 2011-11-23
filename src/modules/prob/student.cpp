/* ----------------------------------------------------------------------- *//** 
 *
 * @file student.cpp
 *
 * @brief Evaluate the Student's-T distribution function using the boost library.
 *
 *//* -------------------------------------------------------------------- */

#include <modules/prob/student.hpp>

#include <float.h>
#include <boost/math/distributions/students_t.hpp>

namespace madlib {
  namespace modules {
	namespace prob {

	  /**
	   * @brief Student's-t cumulative distribution function: In-database interface
	   * 
	   * Compute \f$ Pr[T <= x] \f$ for Student-t distributed T with \f$ \nu \f$
	   * degrees of freedom.
	   *
	   * Note: Implemented using the boost library.
	   */
	  AnyType students_t_cdf(AbstractDBInterface & /* db */, AnyType args) {
		AnyType::iterator   arg(args);
		const double        x  = *arg++;
		const double        df = *arg;

        
		/* We want to ensure df > 0 */
		if (df <= 0)
		  throw std::domain_error("Student's t-distribution undefined for "
								  "degree of freedom <= 0");

		return boost::math::cdf( boost::math::students_t(df), x);
	  }

	  /**
	   * @brief Student's-t probability density function: In-database interface
	   * 
	   * Compute \f$ Pr[T <= t] \f$ for Student-t distributed T with \f$ \nu \f$
	   * degrees of freedom.
	   *
	   * Note: Implemented using the boost library.
	   */
	  AnyType students_t_pdf(AbstractDBInterface & /* db */, AnyType args) {
		AnyType::iterator   arg(args);
		const double        x  = *arg++;
		const double        df = *arg;
        
		/* We want to ensure df > 0 */
		if (df <= 0)
		  throw std::domain_error("Student's t-distribution undefined for "
								  "degree of freedom <= 0");

		return boost::math::pdf( boost::math::students_t(df), x);
	  }

	  /**
	   * @brief Student's-t quantile function: In-database interface
	   * 
	   * Returns the value of the random variable x such that \f$ Pr[T <= x] = p \f$
	   * for Student's-t distributed T with \f$ \nu \f$ degrees of freedom.
	   *
	   * Note: Implemented using the boost library.
	   */
	  AnyType students_t_quantile(AbstractDBInterface & /* db */, AnyType args) {
		AnyType::iterator   arg(args);
		const double        p  = *arg++;
		const double        df = *arg;
        
		/* We want to ensure df > 0 */
		if (df <= 0)
		  throw std::domain_error("Student's t-distribution undefined for "
								  "degree of freedom <= 0");
		if (p < 0 || p > 1)
		  throw std::domain_error("Student's t probability must be in the "
								  "range [0,1]");
		if (p < DBL_EPSILON)
		  return -INFINITY;
		if (p > 1 - DBL_EPSILON)
		  return INFINITY;

		return boost::math::quantile( boost::math::students_t(df), p);
	  }

	} // namespace prob
  } // namespace modules
} // namespace regress
