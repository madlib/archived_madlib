/* ----------------------------------------------------------------------- *//**
 *
 * @file laplace.cpp 
 *
 * @brief Probability density and distribution functions of laplace distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/laplace.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "laplace.hpp" 


#define LAPLACE_DOMAIN_CHECK(location, scale)\
	do {\
		if ( std::isnan(x) || std::isnan(location) || std::isnan(scale) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		\
		if ( !(scale > 0) ) {\
			throw std::domain_error("Laplace distribution is undefined when scale doesn't conform to (scale > 0).");\
		}\
	} while(0)


inline double 
laplace_cdf_imp(double x, double location, double scale) {
	LAPLACE_DOMAIN_CHECK(location, scale);
	
	
	if ( x == -std::numeric_limits<double>::infinity() ) {
		return 0.0;
	}
	else if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::laplace_distribution<>(location, scale), x); 
}

/**
 * @brief laplace distribution cumulative function: In-database interface
 */
AnyType
laplace_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return laplace_cdf_imp(x, location, scale);
}

double
laplace_CDF(double x, double location, double scale) {
	double res = 0;

	try {
		res = laplace_cdf_imp(x, location, scale);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
laplace_pdf_imp(double x, double location, double scale) {
	LAPLACE_DOMAIN_CHECK(location, scale);
	
	
	if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::laplace_distribution<>(location, scale), x); 
}

/**
 * @brief laplace distribution probability density function: In-database interface
 */
AnyType
laplace_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return laplace_pdf_imp(x, location, scale);
}

double
laplace_PDF(double x, double location, double scale) {
	double res = 0;

	try {
		res = laplace_pdf_imp(x, location, scale);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
laplace_quantile_imp(double x, double location, double scale) {
	LAPLACE_DOMAIN_CHECK(location, scale);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of laplace distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return -std::numeric_limits<double>::infinity();
	}
	else if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::laplace_distribution<>(location, scale), x); 
}

/**
 * @brief laplace distribution quantile function: In-database interface
 */
AnyType
laplace_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double location = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return laplace_quantile_imp(x, location, scale);
}

double
laplace_QUANTILE(double x, double location, double scale) {
	double res = 0;

	try {
		res = laplace_quantile_imp(x, location, scale);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
