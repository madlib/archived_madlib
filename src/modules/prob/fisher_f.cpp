/* ----------------------------------------------------------------------- *//**
 *
 * @file fisher_f.cpp 
 *
 * @brief Probability density and distribution functions of Fisher-f distruction imported from Boost.
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
		else if ( !(df1 > 0) ) {\
			throw std::domain_error("Fisher-f distribution is undefined when df1 doesn't conform to (df1 > 0).");\
		}\
		else if ( !(df2 > 0) ) {\
			throw std::domain_error("Fisher-f distribution is undefined when df2 doesn't conform to (df2 > 0).");\
		}\
	} while(0)


inline double 
fisher_f_cdf_imp(double x, double df1, double df2) {
	FISHER_F_DOMAIN_CHECK(df1, df2);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::fisher_f_distribution<>(df1, df2), x); 
}

/**
 * @brief Fisher-f distribution cumulative function: In-database interface
 */
AnyType
fisher_f_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df1 = args[1].getAs<double>();
	double df2 = args[2].getAs<double>();

	return fisher_f_cdf_imp(x, df1, df2);
}

double
fisher_f_CDF(double x, double df1, double df2) {
	double res = 0;

	try {
		res = fisher_f_cdf_imp(x, df1, df2);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
fisher_f_pdf_imp(double x, double df1, double df2) {
	FISHER_F_DOMAIN_CHECK(df1, df2);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( std::isinf(x) ) {
		return 0.0;
	}
	else if ( 0 == x ) {
		/// FIXME
		return df1 > 2 ? 0 : (df1 == 2 ? 1 : std::numeric_limits<double>::infinity());
	}

	return boost::math::pdf(boost::math::fisher_f_distribution<>(df1, df2), x); 
}

/**
 * @brief Fisher-f distribution probability density function: In-database interface
 */
AnyType
fisher_f_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df1 = args[1].getAs<double>();
	double df2 = args[2].getAs<double>();

	return fisher_f_pdf_imp(x, df1, df2);
}

double
fisher_f_PDF(double x, double df1, double df2) {
	double res = 0;

	try {
		res = fisher_f_pdf_imp(x, df1, df2);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
fisher_f_quantile_imp(double x, double df1, double df2) {
	FISHER_F_DOMAIN_CHECK(df1, df2);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of Fisher-f distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return 0;
	}
	else if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::fisher_f_distribution<>(df1, df2), x); 
}

/**
 * @brief Fisher-f distribution quantile function: In-database interface
 */
AnyType
fisher_f_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df1 = args[1].getAs<double>();
	double df2 = args[2].getAs<double>();

	return fisher_f_quantile_imp(x, df1, df2);
}

double
fisher_f_QUANTILE(double x, double df1, double df2) {
	double res = 0;

	try {
		res = fisher_f_quantile_imp(x, df1, df2);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
