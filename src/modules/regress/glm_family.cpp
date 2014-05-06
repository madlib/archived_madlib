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

/**
 * @brief Return the Gaussian family object: family, linkfun, etc
 */
AnyType
gaussian::run(AnyType &args) {
	std::string linkfunc;

	// 1. get the linkfun argument: default value -- "identity"
	if (length(args) == 0) then linkfunc = "identity";
	else linkfunc = args[0].getAs<char*>();
	std::transform(linkfunc.begin(), linkfunc.end(), linkfunc.begin(), ::tolower);

	if (linkfunc != "identity" and linkfunc != "inverse" and linkfunc != "log")
		raise_an_error;

	// 2. prepare function object
	glm_family family("gaussian", linkfunc);

	tuple << "gaussian" << linkfunc << family;
	return tuple;
}

} // namespace regress
} // namespace modules
} // namespace madlib
