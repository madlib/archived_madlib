/* ------------------------------------------------------
 *
 * @file glm_family.cpp
 *
 * @brief Generalized linear regression family functions
 *
 *//* ----------------------------------------------------------------------- */
#include <limits>
#include <algorithm>
#include <string>
#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>
#include <boost/math/distributions.hpp>
#include <boost/algorithm/string.hpp>
#include <modules/prob/student.hpp>
#include "glm_family.hpp"
#include "glm.hpp"

namespace madlib { namespace modules { namespace regress {

Family::Family(const std::string f, const std::string l)
{
    if(!f.compare("gaussian")) {
        if(!l.compare("identical")) {
            linkfun = Family::linkfun_gaussian_identical;
            linkinv = Family::linkinv_gaussian_identical;
        }
        else if(!l.compare("inverse")) {
            linkfun = Family::linkfun_gaussian_inverse;
            linkinv = Family::linkinv_gaussian_inverse;
        }
        else if(!l.compare("log")) {
            linkfun = Family::linkfun_gaussian_log;
            linkinv = Family::linkinv_gaussian_log;
        }
        else
            throw std::runtime_error(boost::str(
                    boost::format("Unknown link function for "
                                  "gaussian family: %s") % l));
    }
    else
        throw std::runtime_error(boost::str(
                boost::format("Unknown family: %1") % f));
}

// linkfun functions
double Family::linkfun_gaussian_identical(double mu) {return mu;}
double Family::linkfun_gaussian_inverse(double mu) { return 1/mu;}
double Family::linkfun_gaussian_log(double mu) {return log(mu);}

// linkinv functions
double Family::linkinv_gaussian_identical(double mu) {return 1/mu;}
double Family::linkinv_gaussian_inverse(double mu) {return mu;}
double Family::linkinv_gaussian_log(double mu) {return 1/log(mu);}


/**
 * @brief Return the Gaussian family object: family, linkfun
 */
AnyType
gaussian::run(AnyType &args) {
	std::string linkfunc;

	// 1. get the linkfun argument: default value -- "identity"
	if (args.isNull()) linkfunc = "identity";
	else linkfunc = args[0].getAs<char*>();
	std::transform(linkfunc.begin(), linkfunc.end(), linkfunc.begin(), ::tolower);

    // 2. return family information
    std::string family = "gaussian";
    AnyType tuple;
	tuple << family << linkfunc;
	return tuple;
}

/**
 * @brief Return the Binomial family object: family, linkfun
 */
AnyType
binomial::run(AnyType &args) {
	std::string linkfunc;

	// 1. get the linkfun argument: default value -- "identity"
	if (args.isNull()) linkfunc = "logit";
	else linkfunc = args[0].getAs<char*>();
	std::transform(linkfunc.begin(), linkfunc.end(), linkfunc.begin(), ::tolower);

    // 3. return family information
    std::string family = "binomial";
    AnyType tuple;
	tuple << family << linkfunc;
	return tuple;
}

} } } // namespace madlib::modules::regress

