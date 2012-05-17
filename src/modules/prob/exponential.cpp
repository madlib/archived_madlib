/* ----------------------------------------------------------------------- *//**
 *
 * @file exponential.cpp 
 *
 * @brief Probability density and distribution functions of exponential distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/exponential.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "exponential.hpp" 


#define EXPONENTIAL_DOMAIN_CHECK(rate)\
	do {\
		if ( std::isnan(x) || std::isnan(rate) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		else if ( !(rate > 0) ) {\
			throw std::domain_error("Exponential distribution is undefined when rate doesn't conform to (rate > 0).");\
		}\
	} while(0)


inline double 
exponential_cdf_imp(double x, double rate) {
	EXPONENTIAL_DOMAIN_CHECK(rate);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::exponential_distribution<>(rate), x); 
}

/**
 * @brief exponential distribution cumulative function: In-database interface
 */
AnyType
exponential_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double rate = args[1].getAs<double>();

	return exponential_cdf_imp(x, rate);
}

double
exponential_CDF(double x, double rate) {
	double res = 0;

	try {
		res = exponential_cdf_imp(x, rate);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
exponential_pdf_imp(double x, double rate) {
	EXPONENTIAL_DOMAIN_CHECK(rate);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::exponential_distribution<>(rate), x); 
}

/**
 * @brief exponential distribution probability density function: In-database interface
 */
AnyType
exponential_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double rate = args[1].getAs<double>();

	return exponential_pdf_imp(x, rate);
}

double
exponential_PDF(double x, double rate) {
	double res = 0;

	try {
		res = exponential_pdf_imp(x, rate);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
exponential_quantile_imp(double x, double rate) {
	EXPONENTIAL_DOMAIN_CHECK(rate);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of exponential distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return 0;
	}
	else if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::exponential_distribution<>(rate), x); 
}

/**
 * @brief exponential distribution quantile function: In-database interface
 */
AnyType
exponential_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double rate = args[1].getAs<double>();

	return exponential_quantile_imp(x, rate);
}

double
exponential_QUANTILE(double x, double rate) {
	double res = 0;

	try {
		res = exponential_quantile_imp(x, rate);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
