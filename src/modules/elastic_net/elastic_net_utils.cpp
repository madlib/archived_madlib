
#include "dbconnector/dbconnector.hpp"
#include "modules/shared/HandleTraits.hpp"
#include "elastic_net_utils.hpp"
#include "share/shared_utils.hpp"

namespace madlib {
namespace modules {
namespace elastic_net {

using namespace madlib::dbal::eigen_integration;

/**
 * @brief Compute the prediction for one point
 */
AnyType __elastic_net_gaussian_predict::run (AnyType& args)
{
    // throws an exception if the coefficients contain NULL values
    try {
        args[0].getAs<MappedColumnVector>();
    } catch(const ArrayWithNullException &e) {
        throw std::runtime_error(
            "Elastic Net error: the coefficients contain NULL values");
    }
    // returns NULL if the feature has NULL values
    try {
        args[2].getAs<MappedColumnVector>();
    } catch(const ArrayWithNullException &e) {
        return Null();
    }

    MappedColumnVector coef = args[0].getAs<MappedColumnVector>();
    MappedColumnVector x = args[2].getAs<MappedColumnVector>();
    double intercept = args[1].getAs<double>();

    double predict = intercept + sparse_dot(coef, x);
    return predict;
}



// ------------------------------------------------------------------------

/**
   @brief Compute the True/False prediction for binomial models
*/
AnyType __elastic_net_binomial_predict::run (AnyType& args)
{
    // throws an exception if the coefficients contain NULL values
    try {
        args[0].getAs<MappedColumnVector>();
    } catch(const ArrayWithNullException &e) {
        throw std::runtime_error(
            "Elastic Net error: the coefficients contain NULL values");
    }
    // returns NULL if the feature has NULL values
    try {
        args[2].getAs<MappedColumnVector>();
    } catch(const ArrayWithNullException &e) {
        return Null();
    }

    MappedColumnVector coef = args[0].getAs<MappedColumnVector>();
    double intercept = args[1].getAs<double>();
    MappedColumnVector x = args[2].getAs<MappedColumnVector>();

    double r = intercept + sparse_dot(coef, x);

    return r > 0;
}

// ------------------------------------------------------------------------

/**
   @brief Compute the probabilities for class True
*/
AnyType __elastic_net_binomial_prob::run (AnyType& args)
{
    // throws an exception if the coefficients contain NULL values
    try {
        args[0].getAs<MappedColumnVector>();
    } catch(const ArrayWithNullException &e) {
        throw std::runtime_error(
            "Elastic Net error: the coefficients contain NULL values");
    }
    // returns NULL if the feature has NULL values
    try {
        args[2].getAs<MappedColumnVector>();
    } catch(const ArrayWithNullException &e) {
        return Null();
    }

    MappedColumnVector coef = args[0].getAs<MappedColumnVector>();
    double intercept = args[1].getAs<double>();
    MappedColumnVector x = args[2].getAs<MappedColumnVector>();

    double r = intercept + sparse_dot(coef, x);
    return 1. / (1 + std::exp(-r));
}

// ------------------------------------------------------------------------

/**
   @brief Compute std::log-likelihood for one data point in binomial models
*/
AnyType __elastic_net_binomial_loglikelihood::run (AnyType& args)
{
    // throws an exception if the coefficients contain NULL values
    try {
        args[0].getAs<MappedColumnVector>();
    } catch(const ArrayWithNullException &e) {
        throw std::runtime_error(
            "Elastic Net error: the coefficients contain NULL values");
    }
    // returns NULL if the feature has NULL values
    try {
        args[3].getAs<MappedColumnVector>();
    } catch(const ArrayWithNullException &e) {
        return Null();
    }

    MappedColumnVector coef = args[0].getAs<MappedColumnVector>();
    double intercept = args[1].getAs<double>();
    MappedColumnVector x = args[3].getAs<MappedColumnVector>();
    double y = args[2].getAs<bool>() ? 1. : -1.;

    double r = intercept + sparse_dot(coef, x);

    if (y > 0)
        return std::log(1 + std::exp(-r));
    else
        return std::log(1 + std::exp(r));
}

}
}
}
