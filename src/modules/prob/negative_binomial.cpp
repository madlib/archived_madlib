/* ----------------------------------------------------------------------- *//**
 *
 * @file negative_binomial.cpp 
 *
 * @brief Probability density and distribution functions of negative_binomial distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/negative_binomial.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "negative_binomial.hpp" 


#define NEGATIVE_BINOMIAL_DOMAIN_CHECK(successes, succ_prob)\
	do {\
		if ( std::isnan(x) || std::isnan(successes) || std::isnan(succ_prob) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		if ( !(successes > 0) ) {\
			throw std::domain_error("Negative_binomial distribution is undefined when successes doesn't conform to (successes > 0).");\
		}\
		if ( !(succ_prob > 0 && succ_prob <= 1) ) {\
			throw std::domain_error("Negative_binomial distribution is undefined when succ_prob doesn't conform to (succ_prob > 0 && succ_prob <= 1).");\
		}\
	} while(0)


inline double 
NEGATIVE_BINOMIAL_CDF(double x, double successes, double succ_prob)
{
	NEGATIVE_BINOMIAL_DOMAIN_CHECK(successes, succ_prob);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	x = floor(x);
	return boost::math::cdf(boost::math::negative_binomial_distribution<>(successes, succ_prob), x); 
}

/**
 * @brief negative_binomial distribution cumulative function: In-database interface
 */
AnyType
negative_binomial_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double successes = args[1].getAs<double>();
	double succ_prob = args[2].getAs<double>();

	return NEGATIVE_BINOMIAL_CDF(x, successes, succ_prob);
}

double
negative_binomial_CDF(double x, double successes, double succ_prob) {
	double res = 0;

	try {
		res = NEGATIVE_BINOMIAL_CDF(x, successes, succ_prob);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
NEGATIVE_BINOMIAL_PDF(double x, double successes, double succ_prob)
{
	NEGATIVE_BINOMIAL_DOMAIN_CHECK(successes, succ_prob);
	if ( (int)x != x && !std::isinf(x) ) {
		throw std::domain_error("Negative_binomial distribution is a discrete distribution, random variable can only be interger.");
	}
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::negative_binomial_distribution<>(successes, succ_prob), x); 
}

/**
 * @brief negative_binomial distribution probability density function: In-database interface
 */
AnyType
negative_binomial_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double successes = args[1].getAs<double>();
	double succ_prob = args[2].getAs<double>();

	return NEGATIVE_BINOMIAL_PDF(x, successes, succ_prob);
}

double
negative_binomial_PDF(double x, double successes, double succ_prob) {
	double res = 0;

	try {
		res = NEGATIVE_BINOMIAL_PDF(x, successes, succ_prob);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
NEGATIVE_BINOMIAL_QUANTILE(double x, double successes, double succ_prob)
{
	NEGATIVE_BINOMIAL_DOMAIN_CHECK(successes, succ_prob);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Negative_binomial distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return 0;
	}
	if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::negative_binomial_distribution<>(successes, succ_prob), x); 
}

/**
 * @brief negative_binomial distribution quantile function: In-database interface
 */
AnyType
negative_binomial_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double successes = args[1].getAs<double>();
	double succ_prob = args[2].getAs<double>();

	return NEGATIVE_BINOMIAL_QUANTILE(x, successes, succ_prob);
}

double
negative_binomial_QUANTILE(double x, double successes, double succ_prob) {
	double res = 0;

	try {
		res = NEGATIVE_BINOMIAL_QUANTILE(x, successes, succ_prob);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
