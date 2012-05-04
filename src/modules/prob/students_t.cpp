/* ----------------------------------------------------------------------- *//**
 *
 * @file students_t.cpp 
 *
 * @brief Probability density and distribution functions of students_t distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/students_t.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "students_t.hpp" 


#define STUDENTS_T_DOMAIN_CHECK(df)\
	do {\
		if ( std::isnan(x) || std::isnan(df) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		if ( !(df > 0) ) {\
			throw std::domain_error("Students_t distribution is undefined when df doesn't conform to (df > 0).");\
		}\
	} while(0)


inline double 
STUDENTS_T_CDF(double x, double df)
{
	STUDENTS_T_DOMAIN_CHECK(df);
	
	
	if ( x == -std::numeric_limits<double>::infinity() ) {
		return 0.0;
	}
	if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::students_t_distribution<>(df), x); 
}

/**
 * @brief students_t distribution cumulative function: In-database interface
 */
AnyType
students_t_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();

	return STUDENTS_T_CDF(x, df);
}

double
students_t_CDF(double x, double df) {
	double res = 0;

	try {
		res = STUDENTS_T_CDF(x, df);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
STUDENTS_T_PDF(double x, double df)
{
	STUDENTS_T_DOMAIN_CHECK(df);
	
	
	if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::students_t_distribution<>(df), x); 
}

/**
 * @brief students_t distribution probability density function: In-database interface
 */
AnyType
students_t_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();

	return STUDENTS_T_PDF(x, df);
}

double
students_t_PDF(double x, double df) {
	double res = 0;

	try {
		res = STUDENTS_T_PDF(x, df);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
STUDENTS_T_QUANTILE(double x, double df)
{
	STUDENTS_T_DOMAIN_CHECK(df);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Students_t distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return -std::numeric_limits<double>::infinity();
	}
	if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::students_t_distribution<>(df), x); 
}

/**
 * @brief students_t distribution quantile function: In-database interface
 */
AnyType
students_t_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();

	return STUDENTS_T_QUANTILE(x, df);
}

double
students_t_QUANTILE(double x, double df) {
	double res = 0;

	try {
		res = STUDENTS_T_QUANTILE(x, df);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
