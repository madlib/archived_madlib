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
		else if ( !(location > 0) ) {\
			throw std::domain_error("Pareto distribution is undefined when location doesn't conform to (location > 0).");\
		}\
		else if ( !(shape > 0) ) {\
			throw std::domain_error("Pareto distribution is undefined when shape doesn't conform to (shape > 0).");\
		}\
	} while(0)


inline double 
pareto_cdf_imp(double x, double location, double shape) {
	PARETO_DOMAIN_CHECK(location, shape);
	
	
	if ( x < location ) {
		return 0.0;
	}
	else if ( x == std::numeric_limits<double>::infinity() ) {
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

	return pareto_cdf_imp(x, location, shape);
}

double
pareto_CDF(double x, double location, double shape) {
	double res = 0;

	try {
		res = pareto_cdf_imp(x, location, shape);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
pareto_pdf_imp(double x, double location, double shape) {
	PARETO_DOMAIN_CHECK(location, shape);
	
	
	if ( x < location ) {
		return 0.0;
	}
	else if ( std::isinf(x) ) {
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

	return pareto_pdf_imp(x, location, shape);
}

double
pareto_PDF(double x, double location, double shape) {
	double res = 0;

	try {
		res = pareto_pdf_imp(x, location, shape);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
pareto_quantile_imp(double x, double location, double shape) {
	PARETO_DOMAIN_CHECK(location, shape);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of pareto distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return location;
	}
	else if ( 1 == x ) {
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

	return pareto_quantile_imp(x, location, shape);
}

double
pareto_QUANTILE(double x, double location, double shape) {
	double res = 0;

	try {
		res = pareto_quantile_imp(x, location, shape);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
