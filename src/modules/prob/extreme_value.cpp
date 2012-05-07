/* ----------------------------------------------------------------------- *//**
 *
 * @file extreme_value.cpp 
 *
 * @brief Probability density and distribution functions of extreme_value distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/extreme_value.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "extreme_value.hpp" 


#define EXTREME_VALUE_DOMAIN_CHECK(location, scale)\
	do {\
		if ( std::isnan(x) || std::isnan(location) || std::isnan(scale) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		\
		if ( !(scale > 0) ) {\
			throw std::domain_error("Extreme_value distribution is undefined when scale doesn't conform to (scale > 0).");\
		}\
	} while(0)


inline double 
EXTREME_VALUE_CDF(double x, double location, double scale)
{
	EXTREME_VALUE_DOMAIN_CHECK(location, scale);
	
	
	if ( x == -std::numeric_limits<double>::infinity() ) {
		return 0.0;
	}
	if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::extreme_value_distribution<>(location, scale), x); 
}

/**
 * @brief extreme_value distribution cumulative function: In-database interface
 */
AnyType
extreme_value_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return EXTREME_VALUE_CDF(x, location, scale);
}

double
extreme_value_CDF(double x, double location, double scale) {
	double res = 0;

	try {
		res = EXTREME_VALUE_CDF(x, location, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
EXTREME_VALUE_PDF(double x, double location, double scale)
{
	EXTREME_VALUE_DOMAIN_CHECK(location, scale);
	
	
	if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::extreme_value_distribution<>(location, scale), x); 
}

/**
 * @brief extreme_value distribution probability density function: In-database interface
 */
AnyType
extreme_value_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return EXTREME_VALUE_PDF(x, location, scale);
}

double
extreme_value_PDF(double x, double location, double scale) {
	double res = 0;

	try {
		res = EXTREME_VALUE_PDF(x, location, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
EXTREME_VALUE_QUANTILE(double x, double location, double scale)
{
	EXTREME_VALUE_DOMAIN_CHECK(location, scale);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Extreme_value distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return -std::numeric_limits<double>::infinity();
	}
	if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::extreme_value_distribution<>(location, scale), x); 
}

/**
 * @brief extreme_value distribution quantile function: In-database interface
 */
AnyType
extreme_value_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return EXTREME_VALUE_QUANTILE(x, location, scale);
}

double
extreme_value_QUANTILE(double x, double location, double scale) {
	double res = 0;

	try {
		res = EXTREME_VALUE_QUANTILE(x, location, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
