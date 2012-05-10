/* ----------------------------------------------------------------------- *//**
 *
 * @file cauchy.cpp 
 *
 * @brief Probability density and distribution functions of cauchy distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/cauchy.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "cauchy.hpp" 


#define CAUCHY_DOMAIN_CHECK(location, scale)\
	do {\
		if ( std::isnan(x) || std::isnan(location) || std::isnan(scale) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		\
		if ( !(scale > 0) ) {\
			throw std::domain_error("Cauchy distribution is undefined when scale doesn't conform to (scale > 0).");\
		}\
	} while(0)


inline double 
_cauchy_cdf(double x, double location, double scale) {
	CAUCHY_DOMAIN_CHECK(location, scale);
	
	
	if ( x == -std::numeric_limits<double>::infinity() ) {
		return 0.0;
	}
	else if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::cauchy_distribution<>(location, scale), x); 
}

/**
 * @brief cauchy distribution cumulative function: In-database interface
 */
AnyType
cauchy_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return _cauchy_cdf(x, location, scale);
}

double
cauchy_CDF(double x, double location, double scale) {
	double res = 0;

	try {
		res = _cauchy_cdf(x, location, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
_cauchy_pdf(double x, double location, double scale) {
	CAUCHY_DOMAIN_CHECK(location, scale);
	
	
	if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::cauchy_distribution<>(location, scale), x); 
}

/**
 * @brief cauchy distribution probability density function: In-database interface
 */
AnyType
cauchy_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return _cauchy_pdf(x, location, scale);
}

double
cauchy_PDF(double x, double location, double scale) {
	double res = 0;

	try {
		res = _cauchy_pdf(x, location, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
_cauchy_quantile(double x, double location, double scale) {
	CAUCHY_DOMAIN_CHECK(location, scale);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of cauchy distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return -std::numeric_limits<double>::infinity();
	}
	else if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::cauchy_distribution<>(location, scale), x); 
}

/**
 * @brief cauchy distribution quantile function: In-database interface
 */
AnyType
cauchy_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return _cauchy_quantile(x, location, scale);
}

double
cauchy_QUANTILE(double x, double location, double scale) {
	double res = 0;

	try {
		res = _cauchy_quantile(x, location, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
