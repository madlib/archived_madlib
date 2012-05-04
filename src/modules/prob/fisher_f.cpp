/* ----------------------------------------------------------------------- *//**
 *
 * @file fisher_f.cpp 
 *
 * @brief Probability density and distribution functions of fisher_f distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/fisher_f.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "fisher_f.hpp" 


#define FISHER_F_DOMAIN_CHECK(df1, df2)\
	do {\
		if ( std::isnan(x) || std::isnan(df1) || std::isnan(df2) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		if ( !(df1 > 0) ) {\
			throw std::domain_error("Fisher_f distribution is undefined when df1 doesn't conform to (df1 > 0).");\
		}\
		if ( !(df2 > 0) ) {\
			throw std::domain_error("Fisher_f distribution is undefined when df2 doesn't conform to (df2 > 0).");\
		}\
	} while(0)


inline double 
FISHER_F_CDF(double x, double df1, double df2)
{
	FISHER_F_DOMAIN_CHECK(df1, df2);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::fisher_f_distribution<>(df1, df2), x); 
}

/**
 * @brief fisher_f distribution cumulative function: In-database interface
 */
AnyType
fisher_f_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df1 = args[1].getAs<double>();
	double df2 = args[2].getAs<double>();

	return FISHER_F_CDF(x, df1, df2);
}

double
fisher_f_CDF(double x, double df1, double df2) {
	double res = 0;

	try {
		res = FISHER_F_CDF(x, df1, df2);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
FISHER_F_PDF(double x, double df1, double df2)
{
	FISHER_F_DOMAIN_CHECK(df1, df2);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( std::isinf(x) ) {
		return 0.0;
	}
	if ( 0 == x ) {
		return df1 > 2 ? 0 : (df1 == 2 ? 1 : std::numeric_limits<double>::infinity());
	}

	return boost::math::pdf(boost::math::fisher_f_distribution<>(df1, df2), x); 
}

/**
 * @brief fisher_f distribution probability density function: In-database interface
 */
AnyType
fisher_f_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df1 = args[1].getAs<double>();
	double df2 = args[2].getAs<double>();

	return FISHER_F_PDF(x, df1, df2);
}

double
fisher_f_PDF(double x, double df1, double df2) {
	double res = 0;

	try {
		res = FISHER_F_PDF(x, df1, df2);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
FISHER_F_QUANTILE(double x, double df1, double df2)
{
	FISHER_F_DOMAIN_CHECK(df1, df2);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Fisher_f distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return 0;
	}
	if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::fisher_f_distribution<>(df1, df2), x); 
}

/**
 * @brief fisher_f distribution quantile function: In-database interface
 */
AnyType
fisher_f_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df1 = args[1].getAs<double>();
	double df2 = args[2].getAs<double>();

	return FISHER_F_QUANTILE(x, df1, df2);
}

double
fisher_f_QUANTILE(double x, double df1, double df2) {
	double res = 0;

	try {
		res = FISHER_F_QUANTILE(x, df1, df2);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
