/* ----------------------------------------------------------------------- *//**
 *
 * @file poisson.cpp 
 *
 * @brief Probability mass and distribution functions of poisson distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/poisson.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "poisson.hpp" 


#define POISSON_DOMAIN_CHECK(mean)\
	do {\
		if ( std::isnan(x) || std::isnan(mean) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		else if ( !(mean > 0) ) {\
			throw std::domain_error("Poisson distribution is undefined when mean doesn't conform to (mean > 0).");\
		}\
	} while(0)


inline double 
poisson_cdf_imp(double x, double mean) {
	POISSON_DOMAIN_CHECK(mean);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	x = floor(x);
	return boost::math::cdf(boost::math::poisson_distribution<>(mean), x); 
}

/**
 * @brief poisson distribution cumulative function: In-database interface
 */
AnyType
poisson_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double mean = args[1].getAs<double>();

	return poisson_cdf_imp(x, mean);
}

double
poisson_CDF(double x, double mean) {
	double res = 0;

	try {
		res = poisson_cdf_imp(x, mean);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
poisson_pdf_imp(int x, double mean) {
	POISSON_DOMAIN_CHECK(mean);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::poisson_distribution<>(mean), x); 
}

/**
 * @brief poisson distribution probability mass function: In-database interface
 */
AnyType
poisson_pdf::run(AnyType &args) {
	int x = args[0].getAs<int>();
	double mean = args[1].getAs<double>();

	return poisson_pdf_imp(x, mean);
}

double
poisson_PDF(int x, double mean) {
	double res = 0;

	try {
		res = poisson_pdf_imp(x, mean);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
poisson_quantile_imp(double x, double mean) {
	POISSON_DOMAIN_CHECK(mean);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of poisson distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return 0;
	}
	else if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::poisson_distribution<>(mean), x); 
}

/**
 * @brief poisson distribution quantile function: In-database interface
 */
AnyType
poisson_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double mean = args[1].getAs<double>();

	return poisson_quantile_imp(x, mean);
}

double
poisson_QUANTILE(double x, double mean) {
	double res = 0;

	try {
		res = poisson_quantile_imp(x, mean);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
