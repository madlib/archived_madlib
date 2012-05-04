/* ----------------------------------------------------------------------- *//**
 *
 * @file binomial.cpp 
 *
 * @brief Probability density and distribution functions of binomial distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/binomial.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "binomial.hpp" 


#define BINOMIAL_DOMAIN_CHECK(trials, succ_prob)\
	do {\
		if ( std::isnan(x) || std::isnan(trials) || std::isnan(succ_prob) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		if ( !(trials >= 0 && (int)trials == trials) ) {\
			throw std::domain_error("Binomial distribution is undefined when trials doesn't conform to (trials >= 0 && (int)trials == trials).");\
		}\
		if ( !(succ_prob >= 0 && succ_prob <= 1) ) {\
			throw std::domain_error("Binomial distribution is undefined when succ_prob doesn't conform to (succ_prob >= 0 && succ_prob <= 1).");\
		}\
	} while(0)


inline double 
BINOMIAL_CDF(double x, double trials, double succ_prob)
{
	BINOMIAL_DOMAIN_CHECK(trials, succ_prob);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( x > trials ) {
		return 1.0;
	}
	x = floor(x);
	return boost::math::cdf(boost::math::binomial_distribution<>(trials, succ_prob), x); 
}

/**
 * @brief binomial distribution cumulative function: In-database interface
 */
AnyType
binomial_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double trials = args[1].getAs<double>();
	double succ_prob = args[2].getAs<double>();

	return BINOMIAL_CDF(x, trials, succ_prob);
}

double
binomial_CDF(double x, double trials, double succ_prob) {
	double res = 0;

	try {
		res = BINOMIAL_CDF(x, trials, succ_prob);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
BINOMIAL_PDF(double x, double trials, double succ_prob)
{
	BINOMIAL_DOMAIN_CHECK(trials, succ_prob);
	if ( (int)x != x && !std::isinf(x) ) {
		throw std::domain_error("Binomial distribution is a discrete distribution, random variable can only be interger.");
	}
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( x > trials ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::binomial_distribution<>(trials, succ_prob), x); 
}

/**
 * @brief binomial distribution probability density function: In-database interface
 */
AnyType
binomial_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double trials = args[1].getAs<double>();
	double succ_prob = args[2].getAs<double>();

	return BINOMIAL_PDF(x, trials, succ_prob);
}

double
binomial_PDF(double x, double trials, double succ_prob) {
	double res = 0;

	try {
		res = BINOMIAL_PDF(x, trials, succ_prob);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
BINOMIAL_QUANTILE(double x, double trials, double succ_prob)
{
	BINOMIAL_DOMAIN_CHECK(trials, succ_prob);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Binomial distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return 0;
	}
	if ( 1 == x ) {
		return trials;
	}
	return boost::math::quantile(boost::math::binomial_distribution<>(trials, succ_prob), x); 
}

/**
 * @brief binomial distribution quantile function: In-database interface
 */
AnyType
binomial_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double trials = args[1].getAs<double>();
	double succ_prob = args[2].getAs<double>();

	return BINOMIAL_QUANTILE(x, trials, succ_prob);
}

double
binomial_QUANTILE(double x, double trials, double succ_prob) {
	double res = 0;

	try {
		res = BINOMIAL_QUANTILE(x, trials, succ_prob);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
