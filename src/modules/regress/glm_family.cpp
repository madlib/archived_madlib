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

namespace madlib { namespace modules { namespace regress {

Family::Family(const std::string f, const std::string l)
{
    switch(f) {
    case "gaussian":
        switch(l) {
        case "identical":
            linkfun = Family::linkfun_gaussian_identical;
            linkinv = Family::linkinv_gaussian_identical;
        case "inverse":
            linkfun = Family::linkfun_gaussian_inverse;
            linkinv = Family::linkinv_gaussian_inverse;
        case "log":
            linkfun = Family::linkfun_gaussian_log;
            linkinv = Family::linkinv_gaussian_log;
        default: 
            throw std::runtime_error(boost::str(
                    boost::format("Unknown link function for "
                                  "gaussian family: %1") % l));
        }
    default:
        throw std::runtime_error(boost::str(
                boost::format("Unknown family: %1") % f));
    }
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
 * @brief Return the Gaussian family object: family, linkfun, etc
 */
AnyType
gaussian::run(AnyType &args) {
	std::string linkfunc;

	// 1. get the linkfun argument: default value -- "identity"
	if (args.isNull()) linkfunc = "identity";
	else linkfunc = args[0].getAs<char*>();
	std::transform(linkfunc.begin(), linkfunc.end(), linkfunc.begin(), ::tolower);

	// 2. prepare function object
    std::string family = "gaussian";
	Family f(family, linkfunc);

    // 3. return family information
    AnyType tuple;
	tuple << family << linkfunc << f;
	return tuple;
}

} } } // namespace madlib::modules::regress
