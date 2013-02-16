
#include "dbconnector/dbconnector.hpp"
#include "elastic_net_utils.hpp"

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

    double predict = intercept + dot(coef, x);
    return predict;
}

}
}
}
