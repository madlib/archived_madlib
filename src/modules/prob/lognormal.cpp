/* ----------------------------------------------------------------------- *//**
 *
 * @file lognormal.cpp 
 *
 * @brief Probability density and distribution functions of lognormal distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/lognormal.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "lognormal.hpp" 


#define LOGNORMAL_DOMAIN_CHECK(location, scale)\
	do {\
		if ( std::isnan(x) || std::isnan(location) || std::isnan(scale) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		\
		if ( !(scale > 0) ) {\
			throw std::domain_error("Lognormal distribution is undefined when scale doesn't conform to (scale > 0).");\
		}\
	} while(0)


inline double 
_lognormal_cdf(double x, double location, double scale) {
	LOGNORMAL_DOMAIN_CHECK(location, scale);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::lognormal_distribution<>(location, scale), x); 
}

/**
 * @brief lognormal distribution cumulative function: In-database interface
 */
AnyType
lognormal_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return _lognormal_cdf(x, location, scale);
}

double
lognormal_CDF(double x, double location, double scale) {
	double res = 0;

	try {
		res = _lognormal_cdf(x, location, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
_lognormal_pdf(double x, double location, double scale) {
	LOGNORMAL_DOMAIN_CHECK(location, scale);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::lognormal_distribution<>(location, scale), x); 
}

/**
 * @brief lognormal distribution probability density function: In-database interface
 */
AnyType
lognormal_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return _lognormal_pdf(x, location, scale);
}

double
lognormal_PDF(double x, double location, double scale) {
	double res = 0;

	try {
		res = _lognormal_pdf(x, location, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
_lognormal_quantile(double x, double location, double scale) {
	LOGNORMAL_DOMAIN_CHECK(location, scale);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of lognormal distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return 0;
	}
	else if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::lognormal_distribution<>(location, scale), x); 
}

/**
 * @brief lognormal distribution quantile function: In-database interface
 */
AnyType
lognormal_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return _lognormal_quantile(x, location, scale);
}

double
lognormal_QUANTILE(double x, double location, double scale) {
	double res = 0;

	try {
		res = _lognormal_quantile(x, location, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
