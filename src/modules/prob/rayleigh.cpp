/* ----------------------------------------------------------------------- *//**
 *
 * @file rayleigh.cpp 
 *
 * @brief Probability density and distribution functions of rayleigh distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/rayleigh.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "rayleigh.hpp" 


#define RAYLEIGH_DOMAIN_CHECK(sigma)\
	do {\
		if ( std::isnan(x) || std::isnan(sigma) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		if ( !(sigma > 0) ) {\
			throw std::domain_error("Rayleigh distribution is undefined when sigma doesn't conform to (sigma > 0).");\
		}\
	} while(0)


inline double 
RAYLEIGH_CDF(double x, double sigma)
{
	RAYLEIGH_DOMAIN_CHECK(sigma);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::rayleigh_distribution<>(sigma), x); 
}

/**
 * @brief rayleigh distribution cumulative function: In-database interface
 */
AnyType
rayleigh_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double sigma = args[1].getAs<double>();

	return RAYLEIGH_CDF(x, sigma);
}

double
rayleigh_CDF(double x, double sigma) {
	double res = 0;

	try {
		res = RAYLEIGH_CDF(x, sigma);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
RAYLEIGH_PDF(double x, double sigma)
{
	RAYLEIGH_DOMAIN_CHECK(sigma);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::rayleigh_distribution<>(sigma), x); 
}

/**
 * @brief rayleigh distribution probability density function: In-database interface
 */
AnyType
rayleigh_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double sigma = args[1].getAs<double>();

	return RAYLEIGH_PDF(x, sigma);
}

double
rayleigh_PDF(double x, double sigma) {
	double res = 0;

	try {
		res = RAYLEIGH_PDF(x, sigma);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
RAYLEIGH_QUANTILE(double x, double sigma)
{
	RAYLEIGH_DOMAIN_CHECK(sigma);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Rayleigh distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return 0;
	}
	if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::rayleigh_distribution<>(sigma), x); 
}

/**
 * @brief rayleigh distribution quantile function: In-database interface
 */
AnyType
rayleigh_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double sigma = args[1].getAs<double>();

	return RAYLEIGH_QUANTILE(x, sigma);
}

double
rayleigh_QUANTILE(double x, double sigma) {
	double res = 0;

	try {
		res = RAYLEIGH_QUANTILE(x, sigma);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
