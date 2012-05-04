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
		if ( !(df > 0) ) {\
			throw std::domain_error("Chi_squared distribution is undefined when df doesn't conform to (df > 0).");\
		}\
	} while(0)


inline double 
CHI_SQUARED_CDF(double x, double df)
{
	CHI_SQUARED_DOMAIN_CHECK(df);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::chi_squared_distribution<>(df), x); 
}

/**
 * @brief chi_squared distribution cumulative function: In-database interface
 */
AnyType
chi_squared_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();

	return CHI_SQUARED_CDF(x, df);
}

double
chi_squared_CDF(double x, double df) {
	double res = 0;

	try {
		res = CHI_SQUARED_CDF(x, df);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
CHI_SQUARED_PDF(double x, double df)
{
	CHI_SQUARED_DOMAIN_CHECK(df);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( std::isinf(x) ) {
		return 0.0;
	}
	if ( df < 2 && 0 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	if ( 2 == df && 0 == x ) {
		return 0.5;
	}

	return boost::math::pdf(boost::math::chi_squared_distribution<>(df), x); 
}

/**
 * @brief chi_squared distribution probability density function: In-database interface
 */
AnyType
chi_squared_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();

	return CHI_SQUARED_PDF(x, df);
}

double
chi_squared_PDF(double x, double df) {
	double res = 0;

	try {
		res = CHI_SQUARED_PDF(x, df);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
CHI_SQUARED_QUANTILE(double x, double df)
{
	CHI_SQUARED_DOMAIN_CHECK(df);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Chi_squared distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return 0;
	}
	if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::chi_squared_distribution<>(df), x); 
}

/**
 * @brief chi_squared distribution quantile function: In-database interface
 */
AnyType
chi_squared_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();

	return CHI_SQUARED_QUANTILE(x, df);
}

double
chi_squared_QUANTILE(double x, double df) {
	double res = 0;

	try {
		res = CHI_SQUARED_QUANTILE(x, df);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
