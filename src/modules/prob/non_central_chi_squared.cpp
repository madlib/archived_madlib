/* ----------------------------------------------------------------------- *//**
 *
 * @file non_central_chi_squared.cpp 
 *
 * @brief Probability density and distribution functions of non_central_chi_squared distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/non_central_chi_squared.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "non_central_chi_squared.hpp" 


#define NON_CENTRAL_CHI_SQUARED_DOMAIN_CHECK(df, non_centrality)\
	do {\
		if ( std::isnan(x) || std::isnan(df) || std::isnan(non_centrality) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		if ( !(df > 0) ) {\
			throw std::domain_error("Non_central_chi_squared distribution is undefined when df doesn't conform to (df > 0).");\
		}\
		if ( !(non_centrality > 0) ) {\
			throw std::domain_error("Non_central_chi_squared distribution is undefined when non_centrality doesn't conform to (non_centrality > 0).");\
		}\
	} while(0)


inline double 
NON_CENTRAL_CHI_SQUARED_CDF(double x, double df, double non_centrality)
{
	NON_CENTRAL_CHI_SQUARED_DOMAIN_CHECK(df, non_centrality);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::non_central_chi_squared_distribution<>(df, non_centrality), x); 
}

/**
 * @brief non_central_chi_squared distribution cumulative function: In-database interface
 */
AnyType
non_central_chi_squared_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();
	double non_centrality = args[2].getAs<double>();

	return NON_CENTRAL_CHI_SQUARED_CDF(x, df, non_centrality);
}

double
non_central_chi_squared_CDF(double x, double df, double non_centrality) {
	double res = 0;

	try {
		res = NON_CENTRAL_CHI_SQUARED_CDF(x, df, non_centrality);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
NON_CENTRAL_CHI_SQUARED_PDF(double x, double df, double non_centrality)
{
	NON_CENTRAL_CHI_SQUARED_DOMAIN_CHECK(df, non_centrality);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	if ( std::isinf(x) ) {
		return 0.0;
	}
	if ( 0 == x && df < 2 ) {
		return std::numeric_limits<double>::infinity();
	}
	if ( 0 == x && df  == 2 ) {
		/// FIXME
		return pow(M_E, -1*non_centrality/2)*0.5;
	}
	return boost::math::pdf(boost::math::non_central_chi_squared_distribution<>(df, non_centrality), x); 
}

/**
 * @brief non_central_chi_squared distribution probability density function: In-database interface
 */
AnyType
non_central_chi_squared_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();
	double non_centrality = args[2].getAs<double>();

	return NON_CENTRAL_CHI_SQUARED_PDF(x, df, non_centrality);
}

double
non_central_chi_squared_PDF(double x, double df, double non_centrality) {
	double res = 0;

	try {
		res = NON_CENTRAL_CHI_SQUARED_PDF(x, df, non_centrality);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
NON_CENTRAL_CHI_SQUARED_QUANTILE(double x, double df, double non_centrality)
{
	NON_CENTRAL_CHI_SQUARED_DOMAIN_CHECK(df, non_centrality);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("Non_central_chi_squared distribution is undefined for CDF out of range [0, 1].");
	}
	if ( 0 == x ) {
		return 0;
	}
	if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::non_central_chi_squared_distribution<>(df, non_centrality), x); 
}

/**
 * @brief non_central_chi_squared distribution quantile function: In-database interface
 */
AnyType
non_central_chi_squared_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();
	double non_centrality = args[2].getAs<double>();

	return NON_CENTRAL_CHI_SQUARED_QUANTILE(x, df, non_centrality);
}

double
non_central_chi_squared_QUANTILE(double x, double df, double non_centrality) {
	double res = 0;

	try {
		res = NON_CENTRAL_CHI_SQUARED_QUANTILE(x, df, non_centrality);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
