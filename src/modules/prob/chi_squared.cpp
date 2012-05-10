/* ----------------------------------------------------------------------- *//**
 *
 * @file chi_squared.cpp 
 *
 * @brief Probability density and distribution functions of chi_squared distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/chi_squared.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "chi_squared.hpp" 


#define CHI_SQUARED_DOMAIN_CHECK(df)\
	do {\
		if ( std::isnan(x) || std::isnan(df) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		else if ( !(df > 0) ) {\
			throw std::domain_error("Chi-squared distribution is undefined when df doesn't conform to (df > 0).");\
		}\
	} while(0)


inline double 
_chi_squared_cdf(double x, double df) {
	CHI_SQUARED_DOMAIN_CHECK(df);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::chi_squared_distribution<>(df), x); 
}

/**
 * @brief Chi-squared distribution cumulative function: In-database interface
 */
AnyType
chi_squared_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();

	return _chi_squared_cdf(x, df);
}

double
chi_squared_CDF(double x, double df) {
	double res = 0;

	try {
		res = _chi_squared_cdf(x, df);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
_chi_squared_pdf(double x, double df) {
	CHI_SQUARED_DOMAIN_CHECK(df);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( std::isinf(x) ) {
		return 0.0;
	}
	else if ( df < 2 && 0 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	else if ( 2 == df && 0 == x ) {
		/// FIXME
		return 0.5;
	}

	return boost::math::pdf(boost::math::chi_squared_distribution<>(df), x); 
}

/**
 * @brief Chi-squared distribution probability density function: In-database interface
 */
AnyType
chi_squared_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();

	return _chi_squared_pdf(x, df);
}

double
chi_squared_PDF(double x, double df) {
	double res = 0;

	try {
		res = _chi_squared_pdf(x, df);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
_chi_squared_quantile(double x, double df) {
	CHI_SQUARED_DOMAIN_CHECK(df);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of Chi-squared distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return 0;
	}
	else if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::chi_squared_distribution<>(df), x); 
}

/**
 * @brief Chi-squared distribution quantile function: In-database interface
 */
AnyType
chi_squared_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();

	return _chi_squared_quantile(x, df);
}

double
chi_squared_QUANTILE(double x, double df) {
	double res = 0;

	try {
		res = _chi_squared_quantile(x, df);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
