/* ----------------------------------------------------------------------- *//**
 *
 * @file metric.cpp
 *
 * @brief Metric operations
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#include "metric.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

namespace linalg {

template <class Derived, class OtherDerived>
std::tuple<Index, double>
closestColumnAndDistance(
    const Eigen::MatrixBase<Derived>& inMatrix,
    const Eigen::MatrixBase<OtherDerived>& inVector,
    FunctionHandle& inMetric) {

    Index closestColumn = 0;
    double minDist = std::numeric_limits<double>::infinity();

    for (Index i = 0; i < inMatrix.cols(); ++i) {
        double currentDist
            = inMetric(inMatrix.col(i), inVector).template getAs<double>();
        if (currentDist < minDist) {
            closestColumn = i;
            minDist = currentDist;
        }
    }

    return std::tuple<Index, double>(closestColumn, minDist);
}

/**
 * @brief Compute the minimum distance between a vector and any column of a
 *     matrix
 */
AnyType
closest_column::run(AnyType& args) {
    MappedMatrix M = args[0].getAs<MappedMatrix>();
    MappedColumnVector x = args[1].getAs<MappedColumnVector>();
    FunctionHandle dist = args[2].getAs<FunctionHandle>();

    std::tuple<Index, double> result = closestColumnAndDistance(M, x, dist);

    AnyType tuple;
    return tuple
        << static_cast<int16_t>(std::get<0>(result))
        << std::get<1>(result);
}


AnyType
norm2::run(AnyType& args) {
    return static_cast<double>(args[0].getAs<MappedColumnVector>().norm());
}

AnyType
norm1::run(AnyType& args) {
    return static_cast<double>(args[0].getAs<MappedColumnVector>().lpNorm<1>());
}

AnyType
dist_norm2::run(AnyType& args) {
    // FIXME: it would be nice to declare this as a template function (so it
    // works for dense and sparse vectors), and the C++ AL takes care of the
    // rest...
    MappedColumnVector x = args[0].getAs<MappedColumnVector>();
    MappedColumnVector y = args[1].getAs<MappedColumnVector>();

    return static_cast<double>( (x-y).norm() );
}

AnyType
dist_norm1::run(AnyType& args) {
    // FIXME: it would be nice to declare this as a template function (so it
    // works for dense and sparse vectors), and the C++ AL takes care of the
    // rest...
    MappedColumnVector x = args[0].getAs<MappedColumnVector>();
    MappedColumnVector y = args[1].getAs<MappedColumnVector>();

    return static_cast<double>( (x-y).lpNorm<1>() );
}

AnyType
squared_dist_norm2::run(AnyType& args) {
    // FIXME: it would be nice to declare this as a template function (so it
    // works for dense and sparse vectors), and the C++ AL takes care of the
    // rest...
    MappedColumnVector x = args[0].getAs<MappedColumnVector>();
    MappedColumnVector y = args[1].getAs<MappedColumnVector>();

    return static_cast<double>( (x-y).squaredNorm() );
}

AnyType
squared_dist_norm1::run(AnyType& args) {
    // FIXME: it would be nice to declare this as a template function (so it
    // works for dense and sparse vectors), and the C++ AL takes care of the
    // rest...
    MappedColumnVector x = args[0].getAs<MappedColumnVector>();
    MappedColumnVector y = args[1].getAs<MappedColumnVector>();
    double l1norm = (x-y).lpNorm<1>();

    return l1norm * l1norm;
}

} // namespace linalg

} // namespace modules

} // namespace regress
