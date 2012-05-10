/* ----------------------------------------------------------------------- *//**
 *
 * @file weibull.cpp 
 *
 * @brief Probability density and distribution functions of weibull distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/weibull.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "weibull.hpp" 


#define WEIBULL_DOMAIN_CHECK(shape, scale)\
	do {\
		if ( std::isnan(x) || std::isnan(shape) || std::isnan(scale) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		else if ( !(shape > 0) ) {\
			throw std::domain_error("Weibull distribution is undefined when shape doesn't conform to (shape > 0).");\
		}\
		else if ( !(scale > 0) ) {\
			throw std::domain_error("Weibull distribution is undefined when scale doesn't conform to (scale > 0).");\
		}\
	} while(0)


inline double 
_weibull_cdf(double x, double shape, double scale) {
	WEIBULL_DOMAIN_CHECK(shape, scale);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::weibull_distribution<>(shape, scale), x); 
}

/**
 * @brief weibull distribution cumulative function: In-database interface
 */
AnyType
weibull_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double shape = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return _weibull_cdf(x, shape, scale);
}

double
weibull_CDF(double x, double shape, double scale) {
	double res = 0;

	try {
		res = _weibull_cdf(x, shape, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
_weibull_pdf(double x, double shape, double scale) {
	WEIBULL_DOMAIN_CHECK(shape, scale);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( std::isinf(x) ) {
		return 0.0;
	}
	else if ( 0 == x && shape < 1 ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::pdf(boost::math::weibull_distribution<>(shape, scale), x); 
}

/**
 * @brief weibull distribution probability density function: In-database interface
 */
AnyType
weibull_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double shape = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return _weibull_pdf(x, shape, scale);
}

double
weibull_PDF(double x, double shape, double scale) {
	double res = 0;

	try {
		res = _weibull_pdf(x, shape, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
_weibull_quantile(double x, double shape, double scale) {
	WEIBULL_DOMAIN_CHECK(shape, scale);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of weibull distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return 0;
	}
	else if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::weibull_distribution<>(shape, scale), x); 
}

/**
 * @brief weibull distribution quantile function: In-database interface
 */
AnyType
weibull_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double shape = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return _weibull_quantile(x, shape, scale);
}

double
weibull_QUANTILE(double x, double shape, double scale) {
	double res = 0;

	try {
		res = _weibull_quantile(x, shape, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
