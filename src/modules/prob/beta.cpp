/* ----------------------------------------------------------------------- *//**
 *
 * @file beta.cpp 
 *
 * @brief Probability density and distribution functions of beta distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/beta.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "beta.hpp" 


#define BETA_DOMAIN_CHECK(alpha, beta)\
	do {\
		if ( std::isnan(x) || std::isnan(alpha) || std::isnan(beta) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		if ( !(alpha > 0) ) {\
			throw std::domain_error("Beta distribution is undefined when alpha doesn't conform to (alpha > 0).");\
		}\
		if ( !(beta > 0) ) {\
			throw std::domain_error("Beta distribution is undefined when beta doesn't conform to (beta > 0).");\
		}\
	} while(0)


inline double 
BETA_CDF(double x, double alpha, double beta)
{
	BETA_DOMAIN_CHECK(alpha, beta);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( x > 1 ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::beta_distribution<>(alpha, beta), x); 
}

/**
 * @brief beta distribution cumulative function: In-database interface
 */
AnyType
beta_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double alpha = args[1].getAs<double>();
	double beta = args[2].getAs<double>();

	return BETA_CDF(x, alpha, beta);
}

double
beta_CDF(double x, double alpha, double beta) {
	double res = 0;

	try {
		res = BETA_CDF(x, alpha, beta);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
BETA_PDF(double x, double alpha, double beta)
{
	BETA_DOMAIN_CHECK(alpha, beta);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( x > 1 ) {
		return 0.0;
	}
	if ( alpha < 1 && 0 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	if ( beta < 1 && 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::pdf(boost::math::beta_distribution<>(alpha, beta), x); 
}

/**
 * @brief beta distribution probability density function: In-database interface
 */
AnyType
beta_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double alpha = args[1].getAs<double>();
	double beta = args[2].getAs<double>();

	return BETA_PDF(x, alpha, beta);
}

double
beta_PDF(double x, double alpha, double beta) {
	double res = 0;

	try {
		res = BETA_PDF(x, alpha, beta);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
BETA_QUANTILE(double x, double alpha, double beta)
{
	BETA_DOMAIN_CHECK(alpha, beta);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Beta distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return 0;
	}
	if ( 1 == x ) {
		return 1;
	}
	return boost::math::quantile(boost::math::beta_distribution<>(alpha, beta), x); 
}

/**
 * @brief beta distribution quantile function: In-database interface
 */
AnyType
beta_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double alpha = args[1].getAs<double>();
	double beta = args[2].getAs<double>();

	return BETA_QUANTILE(x, alpha, beta);
}

double
beta_QUANTILE(double x, double alpha, double beta) {
	double res = 0;

	try {
		res = BETA_QUANTILE(x, alpha, beta);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
