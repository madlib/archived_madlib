/* ----------------------------------------------------------------------- *//**
 *
 * @file non_central_beta.cpp 
 *
 * @brief Probability density and distribution functions of non_central_beta distruction imported from Boost.
 *
 *//* ----------------------------------------------------------------------- */
#include <dbconnector/dbconnector.hpp>

// We use the Boost implementation
#include <boost/math/distributions/non_central_beta.hpp>
#include <cmath>

namespace madlib {

namespace modules {

namespace prob {
// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "non_central_beta.hpp" 


#define NON_CENTRAL_BETA_DOMAIN_CHECK(alpha, beta, lambda)\
	do {\
		if ( std::isnan(x) || std::isnan(alpha) || std::isnan(beta) || std::isnan(lambda) ) {\
			return std::numeric_limits<double>::quiet_NaN();\
		}\
		else if ( !(alpha > 0) ) {\
			throw std::domain_error("Non_central_beta distribution is undefined when alpha doesn't conform to (alpha > 0).");\
		}\
		else if ( !(beta > 0) ) {\
			throw std::domain_error("Non_central_beta distribution is undefined when beta doesn't conform to (beta > 0).");\
		}\
		else if ( !( lambda >= 0) ) {\
			throw std::domain_error("Non_central_beta distribution is undefined when lambda doesn't conform to ( lambda >= 0).");\
		}\
	} while(0)


inline double 
non_central_beta_cdf_imp(double x, double alpha, double beta, double lambda) {
	NON_CENTRAL_BETA_DOMAIN_CHECK(alpha, beta, lambda);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( x > 1 ) {
		return 1.0;
	}
	return boost::math::cdf(boost::math::non_central_beta_distribution<>(alpha, beta, lambda), x); 
}

/**
 * @brief non_central_beta distribution cumulative function: In-database interface
 */
AnyType
non_central_beta_cdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double alpha = args[1].getAs<double>();
	double beta = args[2].getAs<double>();
	double lambda = args[3].getAs<double>();

	return non_central_beta_cdf_imp(x, alpha, beta, lambda);
}

double
non_central_beta_CDF(double x, double alpha, double beta, double lambda) {
	double res = 0;

	try {
		res = non_central_beta_cdf_imp(x, alpha, beta, lambda);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
non_central_beta_pdf_imp(double x, double alpha, double beta, double lambda) {
	NON_CENTRAL_BETA_DOMAIN_CHECK(alpha, beta, lambda);
	
	
	if ( x < 0 ) {
		return 0.0;
	}
	else if ( x > 1 ) {
		return 0.0;
	}
	else if ( 0 == x && alpha < 1 ) {
		return std::numeric_limits<double>::infinity();
	}
	else if ( 1 == x && beta < 1 ) {
		return std::numeric_limits<double>::infinity();
	}
	return boost::math::pdf(boost::math::non_central_beta_distribution<>(alpha, beta, lambda), x); 
}

/**
 * @brief non_central_beta distribution probability density function: In-database interface
 */
AnyType
non_central_beta_pdf::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double alpha = args[1].getAs<double>();
	double beta = args[2].getAs<double>();
	double lambda = args[3].getAs<double>();

	return non_central_beta_pdf_imp(x, alpha, beta, lambda);
}

double
non_central_beta_PDF(double x, double alpha, double beta, double lambda) {
	double res = 0;

	try {
		res = non_central_beta_pdf_imp(x, alpha, beta, lambda);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}



inline double 
non_central_beta_quantile_imp(double x, double alpha, double beta, double lambda) {
	NON_CENTRAL_BETA_DOMAIN_CHECK(alpha, beta, lambda);
	
	if ( x < 0 || x > 1 ) {
		throw std::domain_error("CDF of non_central_beta distribution must be in range [0, 1].");
	}
	else if ( 0 == x ) {
		return 0;
	}
	else if ( 1 == x ) {
		return 1;
	}
	return boost::math::quantile(boost::math::non_central_beta_distribution<>(alpha, beta, lambda), x); 
}

/**
 * @brief non_central_beta distribution quantile function: In-database interface
 */
AnyType
non_central_beta_quantile::run(AnyType &args) {
	double x = args[0].getAs<double>();
	double alpha = args[1].getAs<double>();
	double beta = args[2].getAs<double>();
	double lambda = args[3].getAs<double>();

	return non_central_beta_quantile_imp(x, alpha, beta, lambda);
}

double
non_central_beta_QUANTILE(double x, double alpha, double beta, double lambda) {
	double res = 0;

	try {
		res = non_central_beta_quantile_imp(x, alpha, beta, lambda);
	}
	catch (const std::domain_error & de) {
		res = std::numeric_limits<double>::quiet_NaN();
	}

	return res;
}
}
}
}
