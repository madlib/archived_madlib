/* ----------------------------------------------------------------------- *//**
 *
 * @file uniform.cpp 
 *
 * @brief Probability density and distribution functions of uniform distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/uniform.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "uniform.hpp" 


#define UNIFORM_DOMAIN_CHECK(lower, upper)\
	do {\
		if ( std::isnan(x) || std::isnan(lower) || std::isnan(upper) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		\
		if ( !(lower < upper) ) {\
			throw std::domain_error("Uniform distribution is undefined when upper doesn't conform to (lower < upper).");\
		}\
		else if ( std::isinf(lower) || std::isinf(upper) ) {\
			throw std::domain_error("Uniform distribution is undefined when upper or lower is infinity.");\
		}\
	} while(0)


inline double 
uniform_cdf_imp(double x, double lower, double upper) {
	UNIFORM_DOMAIN_CHECK(lower, upper);
	
	
	if ( x < lower ) {
		return 0.0;
	}
	else if ( x > upper ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::uniform_distribution<>(lower, upper), x); 
}

/**
 * @brief uniform distribution cumulative function: In-database interface
 */
AnyType
uniform_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double lower = args[1].getAs<double>();
	double upper = args[2].getAs<double>();

	return uniform_cdf_imp(x, lower, upper);
}

double
uniform_CDF(double x, double lower, double upper) {
	double res = 0;

	try {
		res = uniform_cdf_imp(x, lower, upper);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
uniform_pdf_imp(double x, double lower, double upper) {
	UNIFORM_DOMAIN_CHECK(lower, upper);
	
	
	if ( x < lower ) {
		return 0.0;
	}
	else if ( x > upper ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::uniform_distribution<>(lower, upper), x); 
}

/**
 * @brief uniform distribution probability density function: In-database interface
 */
AnyType
uniform_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double lower = args[1].getAs<double>();
	double upper = args[2].getAs<double>();

	return uniform_pdf_imp(x, lower, upper);
}

double
uniform_PDF(double x, double lower, double upper) {
	double res = 0;

	try {
		res = uniform_pdf_imp(x, lower, upper);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
uniform_quantile_imp(double x, double lower, double upper) {
	UNIFORM_DOMAIN_CHECK(lower, upper);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of uniform distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return lower;
	}
	else if ( 1 == x ) {
		return upper;
	}
	return boost::math::quantile(boost::math::uniform_distribution<>(lower, upper), x); 
}

/**
 * @brief uniform distribution quantile function: In-database interface
 */
AnyType
uniform_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double lower = args[1].getAs<double>();
	double upper = args[2].getAs<double>();

	return uniform_quantile_imp(x, lower, upper);
}

double
uniform_QUANTILE(double x, double lower, double upper) {
	double res = 0;

	try {
		res = uniform_quantile_imp(x, lower, upper);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
