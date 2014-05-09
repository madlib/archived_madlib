/* ------------------------------------------------------
 *
 * @file glm.cpp
 *
 * @brief Generalized-Linear-Regression functions
 *
 * GLM implemented based on the iteratively-reweighted-least-squares.
 *
 *//* ----------------------------------------------------------------------- */
#include <limits>
#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>
#include <boost/math/distributions.hpp>
#include <modules/prob/student.hpp>
#include "glm_family.hpp"
#include "glm.hpp"

namespace madlib { namespace modules { namespace regress {

// Use Eigen and import names from other MADlib modules
using namespace dbal::eigen_integration;
using dbal::NoSolutionFoundException;

// FIXME this enum should be accessed by all modules that may need grouping
// valid status values
enum { IN_PROCESS, COMPLETED, TERMINATED, NULL_EMPTY };

AnyType
glm::run(AnyType &args)
{
    //char* family  = args[0][0].getAs<char*>();
    //char* linkfun = args[0][1].getAs<char*>();
    //double x = args[1].getAs<double>();
    //Family f(family, linkfun);
    //return f.linkfun(x);
    //return args[1].getAs<double>() + 1;
    return args[0].numFields();
}

AnyType
glm2::run(AnyType &args)
{
    //char* family  = args[0][0].getAs<char*>();
    //char* linkfun = args[0][1].getAs<char*>();
    //double x = args[1].getAs<double>();
    //Family f(family, linkfun);
    //return f.linkfun(x);
    //return args[1].getAs<double>() + 1;
    return args[0];
}

} } } // namespace madlib::module::regress

