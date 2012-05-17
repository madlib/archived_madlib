/* ----------------------------------------------------------------------- *//**
 *
 * @file triangular.cpp 
 *
 * @brief Probability density and distribution functions of triangular distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/triangular.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "triangular.hpp" 


#define TRIANGULAR_DOMAIN_CHECK(lower, mode, upper)\
	do {\
		if ( std::isnan(x) || std::isnan(lower) || std::isnan(mode) || std::isnan(upper) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		else if ( !(lower < upper) ) {\
			throw std::domain_error("Triangular distribution is undefined when lower doesn't conform to (lower < upper).");\
		}\
		else if ( !(mode >= lower && mode <= upper) ) {\
			throw std::domain_error("Triangular distribution is undefined when mode doesn't conform to (mode >= lower && mode <= upper).");\
		}\
		\
	} while(0)


inline double 
triangular_cdf_imp(double x, double lower, double mode, double upper) {
	TRIANGULAR_DOMAIN_CHECK(lower, mode, upper);
	
	
	if ( x < lower ) {
		return 0.0;
	}
	else if ( x > upper ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::triangular_distribution<>(lower, mode, upper), x); 
}

/**
 * @brief triangular distribution cumulative function: In-database interface
 */
AnyType
triangular_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double lower = args[1].getAs<double>();
	double mode = args[2].getAs<double>();
	double upper = args[3].getAs<double>();

	return triangular_cdf_imp(x, lower, mode, upper);
}

double
triangular_CDF(double x, double lower, double mode, double upper) {
	double res = 0;

	try {
		res = triangular_cdf_imp(x, lower, mode, upper);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
triangular_pdf_imp(double x, double lower, double mode, double upper) {
	TRIANGULAR_DOMAIN_CHECK(lower, mode, upper);
	
	
	if ( x < lower ) {
		return 0.0;
	}
	else if ( x > upper ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::triangular_distribution<>(lower, mode, upper), x); 
}

/**
 * @brief triangular distribution probability density function: In-database interface
 */
AnyType
triangular_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double lower = args[1].getAs<double>();
	double mode = args[2].getAs<double>();
	double upper = args[3].getAs<double>();

	return triangular_pdf_imp(x, lower, mode, upper);
}

double
triangular_PDF(double x, double lower, double mode, double upper) {
	double res = 0;

	try {
		res = triangular_pdf_imp(x, lower, mode, upper);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
triangular_quantile_imp(double x, double lower, double mode, double upper) {
	TRIANGULAR_DOMAIN_CHECK(lower, mode, upper);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of triangular distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return lower;
	}
	else if ( 1 == x ) {
		return upper;
	}
	return boost::math::quantile(boost::math::triangular_distribution<>(lower, mode, upper), x); 
}

/**
 * @brief triangular distribution quantile function: In-database interface
 */
AnyType
triangular_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double lower = args[1].getAs<double>();
	double mode = args[2].getAs<double>();
	double upper = args[3].getAs<double>();

	return triangular_quantile_imp(x, lower, mode, upper);
}

double
triangular_QUANTILE(double x, double lower, double mode, double upper) {
	double res = 0;

	try {
		res = triangular_quantile_imp(x, lower, mode, upper);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
