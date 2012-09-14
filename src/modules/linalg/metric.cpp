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

namespace modules {

namespace linalg {

template <class DistanceFunction>
std::tuple<Index, double>
closestColumnAndDistance(
    const MappedMatrix& inMatrix,
    const MappedColumnVector& inVector,
    DistanceFunction& inMetric) {

    Index closestColumn = 0;
    double minDist = std::numeric_limits<double>::infinity();

    for (Index i = 0; i < inMatrix.cols(); ++i) {
        double currentDist
            = AnyType_cast<double>(
                inMetric(MappedColumnVector(inMatrix.col(i)), inVector)
            );
        if (currentDist < minDist) {
            closestColumn = i;
            minDist = currentDist;
        }
    }

    return std::tuple<Index, double>(closestColumn, minDist);
}

double
squaredDistNorm1(
    const MappedColumnVector& inX,
    const MappedColumnVector& inY) {

    double l1norm = (inX - inY).lpNorm<1>();
    return l1norm * l1norm;
}

double
squaredDistNorm2(
    const MappedColumnVector& inX,
    const MappedColumnVector& inY) {

    return (inX - inY).squaredNorm();
}

double
squaredAngle(
    const MappedColumnVector& inX,
    const MappedColumnVector& inY) {

    double cosine = dot(inX, inY) / (inX.norm() * inY.norm());
    if (cosine > 1)
        cosine = 1;
    else if (cosine < -1)
        cosine = -1;
    double angle = std::acos(cosine);
    return angle * angle;
}

double
squaredTanimoto(
    const MappedColumnVector& inX,
    const MappedColumnVector& inY) {

    // Note that this is not a metric in general!
    double dotProduct = dot(inX, inY);
    double tanimoto = inX.squaredNorm() + inY.squaredNorm();
    tanimoto = (tanimoto - 2 * dotProduct) / (tanimoto - dotProduct);
    return tanimoto * tanimoto;
}

/**
 * @brief Compute the minimum distance between a vector and any column of a
 *     matrix
 *
 * This function calls a user-supplied function, for which it does not do
 * garbage collection. It is therefore meant to be called only constantly many
 * times before control is returned to the backend.
 */
AnyType
closest_column::run(AnyType& args) {
    MappedMatrix M = args[0].getAs<MappedMatrix>();
    MappedColumnVector x = args[1].getAs<MappedColumnVector>();
    FunctionHandle dist = args[2].getAs<FunctionHandle>()
        .unsetFunctionCallOptions(FunctionHandle::GarbageCollectionAfterCall);

    std::tuple<Index, double> result;

    // For performance, we cheat here: For the following four distance
    // functions, we take a special shortcut
    // FIXME: FunctionHandle should be tuned so that this shortcut no longer
    //     impacts performance by more than, say, ~10%.
    if (dist.funcPtr() == funcPtr<squared_dist_norm1>())
        result = closestColumnAndDistance(M, x, squaredDistNorm1);
    else if (dist.funcPtr() == funcPtr<squared_dist_norm2>())
        result = closestColumnAndDistance(M, x, squaredDistNorm2);
    else if (dist.funcPtr() == funcPtr<squared_angle>())
        result = closestColumnAndDistance(M, x, squaredAngle);
    else if (dist.funcPtr() == funcPtr<squared_tanimoto>())
        result = closestColumnAndDistance(M, x, squaredTanimoto);
    else
        result = closestColumnAndDistance(M, x, dist);

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
    return squaredDistNorm2(
        args[0].getAs<MappedColumnVector>(),
        args[1].getAs<MappedColumnVector>()
    );
}

AnyType
squared_dist_norm1::run(AnyType& args) {
    // FIXME: it would be nice to declare this as a template function (so it
    // works for dense and sparse vectors), and the C++ AL takes care of the
    // rest...
    return squaredDistNorm1(
        args[0].getAs<MappedColumnVector>(),
        args[1].getAs<MappedColumnVector>()
    );
}

AnyType
squared_angle::run(AnyType& args) {
    return squaredAngle(
        args[0].getAs<MappedColumnVector>(),
        args[1].getAs<MappedColumnVector>()
    );
}

AnyType
squared_tanimoto::run(AnyType& args) {
    return squaredTanimoto(
        args[0].getAs<MappedColumnVector>(),
        args[1].getAs<MappedColumnVector>()
    );
}

} // namespace linalg

} // namespace modules

} // namespace regress
