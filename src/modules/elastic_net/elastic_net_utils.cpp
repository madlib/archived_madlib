
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
    MappedColumnVector coef = args[0].getAs<MappedColumnVector>();
    double intercept = args[1].getAs<double>();
    MappedColumnVector x = args[2].getAs<MappedColumnVector>();

    double predict = intercept + sparse_dot(coef, x);
    return predict;
}

// ------------------------------------------------------------------------

/**
   @brief Compute the True/False prediction for binomial models
*/
AnyType __elastic_net_binomial_predict::run (AnyType& args)
{
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
    MappedColumnVector coef = args[0].getAs<MappedColumnVector>();
    double intercept = args[1].getAs<double>();
    MappedColumnVector x = args[2].getAs<MappedColumnVector>();

    double r = intercept + sparse_dot(coef, x);
    return 1. / (1 + std::exp(-r));
}

// ------------------------------------------------------------------------

/**
   @brief Compute std::log-likelihood for one data point ion binomial models
*/
AnyType __elastic_net_binomial_loglikelihood::run (AnyType& args)
{
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
