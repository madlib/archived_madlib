/* ----------------------------------------------------------------------- *//**
 *
 * @file geometric.cpp 
 *
 * @brief Probability mass and distribution functions of geometric distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/geometric.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "geometric.hpp" 


#define GEOMETRIC_DOMAIN_CHECK(suc_prob)\
	do {\
		if ( std::isnan(x) || std::isnan(suc_prob) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		else if ( !(0 < suc_prob && suc_prob <= 1) ) {\
			throw std::domain_error("Geometric distribution is undefined when suc_prob doesn't conform to (0 < suc_prob <= 1).");\
		}\
	} while(0)


inline double 
geometric_cdf_imp(double x, double suc_prob) {
	GEOMETRIC_DOMAIN_CHECK(suc_prob);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	x = floor(x);
	return boost::math::cdf(boost::math::geometric_distribution<>(suc_prob), x); 
}

/**
 * @brief geometric distribution cumulative function: In-database interface
 */
AnyType
geometric_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double suc_prob = args[1].getAs<double>();

	return geometric_cdf_imp(x, suc_prob);
}

double
geometric_CDF(double x, double suc_prob) {
	double res = 0;

	try {
		res = geometric_cdf_imp(x, suc_prob);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
geometric_pdf_imp(int x, double suc_prob) {
	GEOMETRIC_DOMAIN_CHECK(suc_prob);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::geometric_distribution<>(suc_prob), x); 
}

/**
 * @brief geometric distribution probability mass function: In-database interface
 */
AnyType
geometric_pdf::run(AnyType &args) {
	int x = args[0].getAs<int>();
	double suc_prob = args[1].getAs<double>();

	return geometric_pdf_imp(x, suc_prob);
}

double
geometric_PDF(int x, double suc_prob) {
	double res = 0;

	try {
		res = geometric_pdf_imp(x, suc_prob);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
geometric_quantile_imp(double x, double suc_prob) {
	GEOMETRIC_DOMAIN_CHECK(suc_prob);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of geometric distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return 0;
	}
	else if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::geometric_distribution<>(suc_prob), x); 
}

/**
 * @brief geometric distribution quantile function: In-database interface
 */
AnyType
geometric_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double suc_prob = args[1].getAs<double>();

	return geometric_quantile_imp(x, suc_prob);
}

double
geometric_QUANTILE(double x, double suc_prob) {
	double res = 0;

	try {
		res = geometric_quantile_imp(x, suc_prob);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
