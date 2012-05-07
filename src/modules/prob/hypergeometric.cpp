/* ----------------------------------------------------------------------- *//**
 *
 * @file hypergeometric.cpp 
 *
 * @brief Probability density and distribution functions of hypergeometric distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/hypergeometric.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "hypergeometric.hpp" 


#define HYPERGEOMETRIC_DOMAIN_CHECK(defective, sample_count, total)\
	do {\
		if ( std::isnan(x) || std::isnan(defective) || std::isnan(sample_count) || std::isnan(total) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		if ( !((int)defective == defective && defective >= 0 && defective <= total) ) {\
			throw std::domain_error("Hypergeometric distribution is undefined when defective doesn't conform to ((int)defective == defective && defective >= 0 && defective <= total).");\
		}\
		if ( !((int)sample_count == sample_count && sample_count >= 1 && sample_count <= total) ) {\
			throw std::domain_error("Hypergeometric distribution is undefined when sample_count doesn't conform to ((int)sample_count == sample_count && sample_count >= 1 && sample_count <= total).");\
		}\
		if ( !( total >=1 && (int)total == total) ) {\
			throw std::domain_error("Hypergeometric distribution is undefined when total doesn't conform to ( total >=1 && (int)total == total).");\
		}\
	} while(0)


inline double 
HYPERGEOMETRIC_CDF(double x, double defective, double sample_count, double total)
{
	HYPERGEOMETRIC_DOMAIN_CHECK(defective, sample_count, total);
	
	
	if ( x < std::max<int>(0, sample_count + defective - total) ) {
		return 0.0;
	}
	if ( x > std::min<int>(defective, sample_count) ) {
		return 1.0;
	}
	x = floor(x);
	return boost::math::cdf(boost::math::hypergeometric_distribution<>(defective, sample_count, total), x); 
}

/**
 * @brief hypergeometric distribution cumulative function: In-database interface
 */
AnyType
hypergeometric_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double defective = args[1].getAs<double>();
	double sample_count = args[2].getAs<double>();
	double total = args[3].getAs<double>();

	return HYPERGEOMETRIC_CDF(x, defective, sample_count, total);
}

double
hypergeometric_CDF(double x, double defective, double sample_count, double total) {
	double res = 0;

	try {
		res = HYPERGEOMETRIC_CDF(x, defective, sample_count, total);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
HYPERGEOMETRIC_PDF(double x, double defective, double sample_count, double total)
{
	HYPERGEOMETRIC_DOMAIN_CHECK(defective, sample_count, total);
	if ( (int)x != x && !std::isinf(x) ) {
		throw std::domain_error("Hypergeometric distribution is a discrete distribution, random variable can only be interger.");
	}
	
	if ( x < std::max<int>(0, sample_count + defective - total) ) {
		return 0.0;
	}
	if ( x > std::min<int>(defective, sample_count) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::hypergeometric_distribution<>(defective, sample_count, total), x); 
}

/**
 * @brief hypergeometric distribution probability density function: In-database interface
 */
AnyType
hypergeometric_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double defective = args[1].getAs<double>();
	double sample_count = args[2].getAs<double>();
	double total = args[3].getAs<double>();

	return HYPERGEOMETRIC_PDF(x, defective, sample_count, total);
}

double
hypergeometric_PDF(double x, double defective, double sample_count, double total) {
	double res = 0;

	try {
		res = HYPERGEOMETRIC_PDF(x, defective, sample_count, total);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
HYPERGEOMETRIC_QUANTILE(double x, double defective, double sample_count, double total)
{
	HYPERGEOMETRIC_DOMAIN_CHECK(defective, sample_count, total);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Hypergeometric distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return std::max<int>(0, sample_count + defective - total);
	}
	if ( 1 == x ) {
		return std::min<int>(defective, sample_count);
	}
	return boost::math::quantile(boost::math::hypergeometric_distribution<>(defective, sample_count, total), x); 
}

/**
 * @brief hypergeometric distribution quantile function: In-database interface
 */
AnyType
hypergeometric_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double defective = args[1].getAs<double>();
	double sample_count = args[2].getAs<double>();
	double total = args[3].getAs<double>();

	return HYPERGEOMETRIC_QUANTILE(x, defective, sample_count, total);
}

double
hypergeometric_QUANTILE(double x, double defective, double sample_count, double total) {
	double res = 0;

	try {
		res = HYPERGEOMETRIC_QUANTILE(x, defective, sample_count, total);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
