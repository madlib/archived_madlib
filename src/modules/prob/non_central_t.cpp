/* ----------------------------------------------------------------------- *//**
 *
 * @file non_central_t.cpp 
 *
 * @brief Probability density and distribution functions of non_central_t distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/non_central_t.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "non_central_t.hpp" 


#define NON_CENTRAL_T_DOMAIN_CHECK(df, non_centrality)\
	do {\
		if ( std::isnan(x) || std::isnan(df) || std::isnan(non_centrality) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		else if ( !(df > 0) ) {\
			throw std::domain_error("Non_central_t distribution is undefined when df doesn't conform to (df > 0).");\
		}\
		\
	} while(0)


inline double 
_non_central_t_cdf(double x, double df, double non_centrality) {
	NON_CENTRAL_T_DOMAIN_CHECK(df, non_centrality);
	
	
	if ( x == -std::numeric_limits<double>::infinity() ) {
		return 0.0;
	}
	else if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::non_central_t_distribution<>(df, non_centrality), x); 
}

/**
 * @brief non_central_t distribution cumulative function: In-database interface
 */
AnyType
non_central_t_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();
	double non_centrality = args[2].getAs<double>();

	return _non_central_t_cdf(x, df, non_centrality);
}

double
non_central_t_CDF(double x, double df, double non_centrality) {
	double res = 0;

	try {
		res = _non_central_t_cdf(x, df, non_centrality);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
_non_central_t_pdf(double x, double df, double non_centrality) {
	NON_CENTRAL_T_DOMAIN_CHECK(df, non_centrality);
	
	
	if ( std::isinf(x) ) {
		return 0.0;
	}
	return boost::math::pdf(boost::math::non_central_t_distribution<>(df, non_centrality), x); 
}

/**
 * @brief non_central_t distribution probability density function: In-database interface
 */
AnyType
non_central_t_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();
	double non_centrality = args[2].getAs<double>();

	return _non_central_t_pdf(x, df, non_centrality);
}

double
non_central_t_PDF(double x, double df, double non_centrality) {
	double res = 0;

	try {
		res = _non_central_t_pdf(x, df, non_centrality);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
_non_central_t_quantile(double x, double df, double non_centrality) {
	NON_CENTRAL_T_DOMAIN_CHECK(df, non_centrality);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of non_central_t distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return -std::numeric_limits<double>::infinity();
	}
	else if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::non_central_t_distribution<>(df, non_centrality), x); 
}

/**
 * @brief non_central_t distribution quantile function: In-database interface
 */
AnyType
non_central_t_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df = args[1].getAs<double>();
	double non_centrality = args[2].getAs<double>();

	return _non_central_t_quantile(x, df, non_centrality);
}

double
non_central_t_QUANTILE(double x, double df, double non_centrality) {
	double res = 0;

	try {
		res = _non_central_t_quantile(x, df, non_centrality);
	}
	catch (...) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
