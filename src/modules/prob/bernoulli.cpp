/* ----------------------------------------------------------------------- *//**
 *
 * @file bernoulli.cpp 
 *
 * @brief Probability density and distribution functions of bernoulli distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/bernoulli.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "bernoulli.hpp" 


#define BERNOULLI_DOMAIN_CHECK(succ_prob)\
	do {\
		if ( std::isnan(x) || std::isnan(succ_prob) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		if ( !(succ_prob >= 0 && succ_prob <= 1) ) {\
			throw std::domain_error("Bernoulli distribution is undefined when succ_prob doesn't conform to (succ_prob >= 0 && succ_prob <= 1).");\
		}\
	} while(0)


inline double 
BERNOULLI_CDF(double x, double succ_prob)
{
	BERNOULLI_DOMAIN_CHECK(succ_prob);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( x > 1 ) {
		return 1.0;
	}
	x = floor(x);
	return boost::math::cdf(boost::math::bernoulli_distribution<>(succ_prob), x); 
}

/**
 * @brief bernoulli distribution cumulative function: In-database interface
 */
AnyType
bernoulli_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double succ_prob = args[1].getAs<double>();

	return BERNOULLI_CDF(x, succ_prob);
}

double
bernoulli_CDF(double x, double succ_prob) {
	double res = 0;

	try {
		res = BERNOULLI_CDF(x, succ_prob);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
BERNOULLI_PDF(double x, double succ_prob)
{
	BERNOULLI_DOMAIN_CHECK(succ_prob);
	if ( (int)x != x && !std::isinf(x) ) {
		throw std::domain_error("Bernoulli distribution is a discrete distribution, random variable can only be interger.");
	}
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( x > 1 ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::bernoulli_distribution<>(succ_prob), x); 
}

/**
 * @brief bernoulli distribution probability density function: In-database interface
 */
AnyType
bernoulli_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double succ_prob = args[1].getAs<double>();

	return BERNOULLI_PDF(x, succ_prob);
}

double
bernoulli_PDF(double x, double succ_prob) {
	double res = 0;

	try {
		res = BERNOULLI_PDF(x, succ_prob);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
BERNOULLI_QUANTILE(double x, double succ_prob)
{
	BERNOULLI_DOMAIN_CHECK(succ_prob);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Bernoulli distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return 0;
	}
	if ( 1 == x ) {
		return 1;
	}
	return boost::math::quantile(boost::math::bernoulli_distribution<>(succ_prob), x); 
}

/**
 * @brief bernoulli distribution quantile function: In-database interface
 */
AnyType
bernoulli_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double succ_prob = args[1].getAs<double>();

	return BERNOULLI_QUANTILE(x, succ_prob);
}

double
bernoulli_QUANTILE(double x, double succ_prob) {
	double res = 0;

	try {
		res = BERNOULLI_QUANTILE(x, succ_prob);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
