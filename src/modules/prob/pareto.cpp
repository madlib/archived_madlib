/* ----------------------------------------------------------------------- *//**
 *
 * @file pareto.cpp 
 *
 * @brief Probability density and distribution functions of pareto distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/pareto.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "pareto.hpp" 


#define PARETO_DOMAIN_CHECK(location, shape)\
	do {\
		if ( std::isnan(x) || std::isnan(location) || std::isnan(shape) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		if ( !(location > 0) ) {\
			throw std::domain_error("Pareto distribution is undefined when location doesn't conform to (location > 0).");\
		}\
		if ( !(shape > 0) ) {\
			throw std::domain_error("Pareto distribution is undefined when shape doesn't conform to (shape > 0).");\
		}\
	} while(0)


inline double 
PARETO_CDF(double x, double location, double shape)
{
	PARETO_DOMAIN_CHECK(location, shape);
	
	
	if ( x < location ) {
		return 0.0;
	}
	if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::pareto_distribution<>(location, shape), x); 
}

/**
 * @brief pareto distribution cumulative function: In-database interface
 */
AnyType
pareto_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double shape = args[2].getAs<double>();

	return PARETO_CDF(x, location, shape);
}

double
pareto_CDF(double x, double location, double shape) {
	double res = 0;

	try {
		res = PARETO_CDF(x, location, shape);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
PARETO_PDF(double x, double location, double shape)
{
	PARETO_DOMAIN_CHECK(location, shape);
	
	
	if ( x < location ) {
		return 0.0;
	}
	if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::pareto_distribution<>(location, shape), x); 
}

/**
 * @brief pareto distribution probability density function: In-database interface
 */
AnyType
pareto_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double shape = args[2].getAs<double>();

	return PARETO_PDF(x, location, shape);
}

double
pareto_PDF(double x, double location, double shape) {
	double res = 0;

	try {
		res = PARETO_PDF(x, location, shape);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
PARETO_QUANTILE(double x, double location, double shape)
{
	PARETO_DOMAIN_CHECK(location, shape);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Pareto distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return location;
	}
	if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::pareto_distribution<>(location, shape), x); 
}

/**
 * @brief pareto distribution quantile function: In-database interface
 */
AnyType
pareto_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double shape = args[2].getAs<double>();

	return PARETO_QUANTILE(x, location, shape);
}

double
pareto_QUANTILE(double x, double location, double shape) {
	double res = 0;

	try {
		res = PARETO_QUANTILE(x, location, shape);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
