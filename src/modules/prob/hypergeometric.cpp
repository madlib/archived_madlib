/* ----------------------------------------------------------------------- *//**
 *
 * @file hypergeometric.cpp 
 *
 * @brief Probability mass and distribution functions of hypergeometric distruction imported from Boost.
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
		else if ( !( defective >= 0 && defective <= total) ) {\
			throw std::domain_error("Hypergeometric distribution is undefined when defective doesn't conform to (defective >= 0 && defective <= total).");\
		}\
		else if ( !( sample_count >= 1 && sample_count <= total) ) {\
			throw std::domain_error("Hypergeometric distribution is undefined when sample_count doesn't conform to (sample_count >= 1 && sample_count <= total).");\
		}\
		else if ( !( total >=1 ) ) {\
			throw std::domain_error("Hypergeometric distribution is undefined when total doesn't conform to ( total >=1 ).");\
		}\
	} while(0)


inline double 
_hypergeometric_cdf(double x, int defective, int sample_count, int total) {
	HYPERGEOMETRIC_DOMAIN_CHECK(defective, sample_count, total);
	
	
	if ( x < std::max<int>(0, sample_count + defective - total) ) {
		return 0.0;
	}
	else if ( x > std::min<int>(defective, sample_count) ) {
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
	int defective = args[1].getAs<int>();
	int sample_count = args[2].getAs<int>();
	int total = args[3].getAs<int>();

	return _hypergeometric_cdf(x, defective, sample_count, total);
}

double
hypergeometric_CDF(double x, int defective, int sample_count, int total) {
	double res = 0;

	try {
		res = _hypergeometric_cdf(x, defective, sample_count, total);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
_hypergeometric_pdf(int x, int defective, int sample_count, int total) {
	HYPERGEOMETRIC_DOMAIN_CHECK(defective, sample_count, total);
	
	
	if ( x < std::max<int>(0, sample_count + defective - total) ) {
		return 0.0;
	}
	else if ( x > std::min<int>(defective, sample_count) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::hypergeometric_distribution<>(defective, sample_count, total), x); 
}

/**
 * @brief hypergeometric distribution probability mass function: In-database interface
 */
AnyType
hypergeometric_pdf::run(AnyType &args) {
	int x = args[0].getAs<int>();
	int defective = args[1].getAs<int>();
	int sample_count = args[2].getAs<int>();
	int total = args[3].getAs<int>();

	return _hypergeometric_pdf(x, defective, sample_count, total);
}

double
hypergeometric_PDF(int x, int defective, int sample_count, int total) {
	double res = 0;

	try {
		res = _hypergeometric_pdf(x, defective, sample_count, total);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
_hypergeometric_quantile(double x, int defective, int sample_count, int total) {
	HYPERGEOMETRIC_DOMAIN_CHECK(defective, sample_count, total);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of hypergeometric distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return std::max<int>(0, sample_count + defective - total);
	}
	else if ( 1 == x ) {
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
	int defective = args[1].getAs<int>();
	int sample_count = args[2].getAs<int>();
	int total = args[3].getAs<int>();

	return _hypergeometric_quantile(x, defective, sample_count, total);
}

double
hypergeometric_QUANTILE(double x, int defective, int sample_count, int total) {
	double res = 0;

	try {
		res = _hypergeometric_quantile(x, defective, sample_count, total);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
