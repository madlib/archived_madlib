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

namespace madlib {
namespace modules {
namespace regress {

family::family(const std::string f, const std::string l)
{
    switch(f) {
    case "gaussian":
        switch(l) {
        case "identical":
            linkfun = family::linkfun_gaussian_identical;
            linkinv = family::linkinv_gaussian_identical;
        case "inverse":
            linkfun = family::linkfun_gaussian_inverse;
            linkinv = family::linkinv_gaussian_inverse;
        case "log":
            linkfun = family::linkfun_gaussian_log;
            linkinv = family::linkinv_gaussian_log;
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
double family::linkfun_gaussian_identical(double mu) {return mu;}
double family::linkfun_gaussian_inverse(double mu) { return 1/mu;}
double family::linkfun_gaussian_log(double mu) {return log(mu);}

// linkinv functions
double family::linkinv_gaussian_identical(double mu) {return mu;}
double family::linkinv_gaussian_inverse(double mu) {return mu;}
double family::linkinv_gaussian_log(double mu) {return 1/log(mu);}

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
    std::string ftype = "gaussian";
	family f(ftype, linkfunc);

    // 3. return family information
    AnyType tuple;
	tuple << ftype << linkfunc << f;
	return tuple;
}

} // namespace regress
} // namespace modules
} // namespace madlib

