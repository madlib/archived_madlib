/* ----------------------------------------------------------------------- *//**
 *
 * @file normal.cpp 
 *
 * @brief Probability density and distribution functions of normal distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/normal.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "normal.hpp" 


#define NORMAL_DOMAIN_CHECK(mean, sd)\
	do {\
		if ( std::isnan(x) || std::isnan(mean) || std::isnan(sd) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		\
		if ( !(sd > 0) ) {\
			throw std::domain_error("Normal distribution is undefined when sd doesn't conform to (sd > 0).");\
		}\
	} while(0)


inline double 
NORMAL_CDF(double x, double mean, double sd)
{
	NORMAL_DOMAIN_CHECK(mean, sd);
	
	
	if ( x == -std::numeric_limits<double>::infinity() ) {
		return 0.0;
	}
	if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::normal_distribution<>(mean, sd), x); 
}

/**
 * @brief normal distribution cumulative function: In-database interface
 */
AnyType
normal_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double mean = args[1].getAs<double>();
	double sd = args[2].getAs<double>();

	return NORMAL_CDF(x, mean, sd);
}

double
normal_CDF(double x, double mean, double sd) {
	double res = 0;

	try {
		res = NORMAL_CDF(x, mean, sd);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
NORMAL_PDF(double x, double mean, double sd)
{
	NORMAL_DOMAIN_CHECK(mean, sd);
	
	
	if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::normal_distribution<>(mean, sd), x); 
}

/**
 * @brief normal distribution probability density function: In-database interface
 */
AnyType
normal_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double mean = args[1].getAs<double>();
	double sd = args[2].getAs<double>();

	return NORMAL_PDF(x, mean, sd);
}

double
normal_PDF(double x, double mean, double sd) {
	double res = 0;

	try {
		res = NORMAL_PDF(x, mean, sd);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
NORMAL_QUANTILE(double x, double mean, double sd)
{
	NORMAL_DOMAIN_CHECK(mean, sd);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Normal distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return -std::numeric_limits<double>::infinity();
	}
	if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::normal_distribution<>(mean, sd), x); 
}

/**
 * @brief normal distribution quantile function: In-database interface
 */
AnyType
normal_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double mean = args[1].getAs<double>();
	double sd = args[2].getAs<double>();

	return NORMAL_QUANTILE(x, mean, sd);
}

double
normal_QUANTILE(double x, double mean, double sd) {
	double res = 0;

	try {
		res = NORMAL_QUANTILE(x, mean, sd);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
