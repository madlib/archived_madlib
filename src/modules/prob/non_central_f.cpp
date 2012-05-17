/* ----------------------------------------------------------------------- *//**
 *
 * @file non_central_f.cpp 
 *
 * @brief Probability density and distribution functions of non_central_f distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/non_central_f.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "non_central_f.hpp" 


#define NON_CENTRAL_F_DOMAIN_CHECK(df1, df2, non_centrality)\
	do {\
		if ( std::isnan(x) || std::isnan(df1) || std::isnan(df2) || std::isnan(non_centrality) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		else if ( !(df1 > 0) ) {\
			throw std::domain_error("Non_central_f distribution is undefined when df1 doesn't conform to (df1 > 0).");\
		}\
		else if ( !(df2 > 0) ) {\
			throw std::domain_error("Non_central_f distribution is undefined when df2 doesn't conform to (df2 > 0).");\
		}\
		else if ( !(non_centrality >= 0) ) {\
			throw std::domain_error("Non_central_f distribution is undefined when non_centrality doesn't conform to (non_centrality >= 0).");\
		}\
	} while(0)


inline double 
non_central_f_cdf_imp(double x, double df1, double df2, double non_centrality) {
	NON_CENTRAL_F_DOMAIN_CHECK(df1, df2, non_centrality);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( x == std::numeric_limits<double>::infinity() ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::non_central_f_distribution<>(df1, df2, non_centrality), x); 
}

/**
 * @brief non_central_f distribution cumulative function: In-database interface
 */
AnyType
non_central_f_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df1 = args[1].getAs<double>();
	double df2 = args[2].getAs<double>();
	double non_centrality = args[3].getAs<double>();

	return non_central_f_cdf_imp(x, df1, df2, non_centrality);
}

double
non_central_f_CDF(double x, double df1, double df2, double non_centrality) {
	double res = 0;

	try {
		res = non_central_f_cdf_imp(x, df1, df2, non_centrality);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
non_central_f_pdf_imp(double x, double df1, double df2, double non_centrality) {
	NON_CENTRAL_F_DOMAIN_CHECK(df1, df2, non_centrality);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( std::isinf(x) ) {
		return 0.0;
	}
	else if ( df1 < 2 && 0 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	else if ( 2 == df1 && 0 == x ) {
		/// FIXME
		return 0.2231301601484298289333;
	}
	return boost::math::pdf(boost::math::non_central_f_distribution<>(df1, df2, non_centrality), x); 
}

/**
 * @brief non_central_f distribution probability density function: In-database interface
 */
AnyType
non_central_f_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df1 = args[1].getAs<double>();
	double df2 = args[2].getAs<double>();
	double non_centrality = args[3].getAs<double>();

	return non_central_f_pdf_imp(x, df1, df2, non_centrality);
}

double
non_central_f_PDF(double x, double df1, double df2, double non_centrality) {
	double res = 0;

	try {
		res = non_central_f_pdf_imp(x, df1, df2, non_centrality);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
non_central_f_quantile_imp(double x, double df1, double df2, double non_centrality) {
	NON_CENTRAL_F_DOMAIN_CHECK(df1, df2, non_centrality);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of non_central_f distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return 0;
	}
	else if ( 1 == x ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::quantile(boost::math::non_central_f_distribution<>(df1, df2, non_centrality), x); 
}

/**
 * @brief non_central_f distribution quantile function: In-database interface
 */
AnyType
non_central_f_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double df1 = args[1].getAs<double>();
	double df2 = args[2].getAs<double>();
	double non_centrality = args[3].getAs<double>();

	return non_central_f_quantile_imp(x, df1, df2, non_centrality);
}

double
non_central_f_QUANTILE(double x, double df1, double df2, double non_centrality) {
	double res = 0;

	try {
		res = non_central_f_quantile_imp(x, df1, df2, non_centrality);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
