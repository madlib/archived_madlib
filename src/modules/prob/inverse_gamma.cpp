/* ----------------------------------------------------------------------- *//**
 *
 * @file inverse_gamma.cpp 
 *
 * @brief Probability density and distribution functions of inverse_gamma distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/inverse_gamma.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "inverse_gamma.hpp" 


#define INVERSE_GAMMA_DOMAIN_CHECK(shape, scale)\
	do {\
		if ( std::isnan(x) || std::isnan(shape) || std::isnan(scale) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		if ( !(shape > 0) ) {\
			throw std::domain_error("Inverse_gamma distribution is undefined when shape doesn't conform to (shape > 0).");\
		}\
		if ( !(scale > 0) ) {\
			throw std::domain_error("Inverse_gamma distribution is undefined when scale doesn't conform to (scale > 0).");\
		}\
	} while(0)


inline double 
INVERSE_GAMMA_CDF(double x, double shape, double scale)
{
	INVERSE_GAMMA_DOMAIN_CHECK(shape, scale);
	if ( !(x > 0) ) {
		throw std::domain_error("Inverse_gamma distribution is undefined when random variable doesn't conform to (x > 0).");
	}
	if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::inverse_gamma_distribution<>(shape, scale), x); 
}

/**
 * @brief inverse_gamma distribution cumulative function: In-database interface
 */
AnyType
inverse_gamma_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double shape = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return INVERSE_GAMMA_CDF(x, shape, scale);
}

double
inverse_gamma_CDF(double x, double shape, double scale) {
	double res = 0;

	try {
		res = INVERSE_GAMMA_CDF(x, shape, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
INVERSE_GAMMA_PDF(double x, double shape, double scale)
{
	INVERSE_GAMMA_DOMAIN_CHECK(shape, scale);
	if ( !(x > 0) ) {
		throw std::domain_error("Inverse_gamma distribution is undefined when random variable doesn't conform to (x > 0).");
	}
	if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::inverse_gamma_distribution<>(shape, scale), x); 
}

/**
 * @brief inverse_gamma distribution probability density function: In-database interface
 */
AnyType
inverse_gamma_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double shape = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return INVERSE_GAMMA_PDF(x, shape, scale);
}

double
inverse_gamma_PDF(double x, double shape, double scale) {
	double res = 0;

	try {
		res = INVERSE_GAMMA_PDF(x, shape, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
INVERSE_GAMMA_QUANTILE(double x, double shape, double scale)
{
	INVERSE_GAMMA_DOMAIN_CHECK(shape, scale);
	
	if ( x <= 0 || x > 1 ) {
		throw std::domain_error("Inverse_gamma distribution is undefined for CDF out of range (0, 1].");
	}
	if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::inverse_gamma_distribution<>(shape, scale), x); 
}

/**
 * @brief inverse_gamma distribution quantile function: In-database interface
 */
AnyType
inverse_gamma_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double shape = args[1].getAs<double>();
	double scale = args[2].getAs<double>();

	return INVERSE_GAMMA_QUANTILE(x, shape, scale);
}

double
inverse_gamma_QUANTILE(double x, double shape, double scale) {
	double res = 0;

	try {
		res = INVERSE_GAMMA_QUANTILE(x, shape, scale);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
