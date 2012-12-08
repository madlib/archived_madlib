/* ----------------------------------------------------------------------- *//**
 *
 * @file metric.cpp
 *
 * @brief Metric operations
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <limits>

#include "metric.hpp"

namespace madlib {

namespace modules {

namespace linalg {

namespace {

template <class TupleType>
struct ReverseLexicographicComparator {
    /**
     * @brief Return true if the first argument is less than the second
     */
    bool operator()(const TupleType& inTuple1, const TupleType& inTuple2) {
        // This could be a real reverse lexicographic comparator in C++11, but
        // lacking variadic template arguments, we simply pretend here that
        // all tuples contain only 2 elements.
        return std::get<1>(inTuple1) < std::get<1>(inTuple2) ||
            (std::get<1>(inTuple1) == std::get<1>(inTuple2) &&
                std::get<0>(inTuple1) < std::get<0>(inTuple2));
    }
};

} // anonymous namespace

/**
 * @brief Compute the k columns of a matrix that are closest to a vector
 *
 * @tparam DistanceFunction Type of the distance function. This can be a
 *     function pointer, or a class with method <tt>operator()</tt> (like
 *     \c FunctionHandle)
 * @tparam NumClosestColumns The number of closest columns to compute. We assume
 *     that \c outIndicesAndDistances is of size \c NumClosestColumns.
 *
 * @param inMatrix Matrix \f$ M \f$
 * @param inVector Vector \f$ \vec x \f$
 * @param[out] outClosestColumns A list of \c NumClosestColumns pairs
 *     \f$ (i, d) \f$ sorted in ascending order of \f$ d \f$, where $i$ is a
 *     0-based column index in \f$ M \f$ and
 *     \f$ d \f$ is the distance (using \c inMetric) between \f$ M_i \f$ and
 *     \f$ x \f$.
 */
template <class DistanceFunction, class RandomAccessIterator>
void
closestColumnsAndDistances(
    const MappedMatrix& inMatrix,
    const MappedColumnVector& inVector,
    DistanceFunction& inMetric,
    RandomAccessIterator ioFirst,
    RandomAccessIterator ioLast) {

    ReverseLexicographicComparator<
        typename std::iterator_traits<RandomAccessIterator>::value_type>
            comparator;

    std::fill(ioFirst, ioLast,
        std::make_tuple(0, std::numeric_limits<double>::infinity()));
    for (Index i = 0; i < inMatrix.cols(); ++i) {
        double currentDist
            = AnyType_cast<double>(
                inMetric(MappedColumnVector(inMatrix.col(i)), inVector)
            );

        // outIndicesAndDistances is a heap, so the first element is maximal
        if (currentDist < std::get<1>(*ioFirst)) {
            // Unfortunately, the STL does not have a decrease-key function,
            // so we are wasting a bit of performance here
            std::pop_heap(ioFirst, ioLast, comparator);
            *(ioLast - 1) = std::make_tuple(i, currentDist);
            std::push_heap(ioFirst, ioLast, comparator);
        }
    }
    std::sort_heap(ioFirst, ioLast, comparator);
}

double
distNorm1(
    const MappedColumnVector& inX,
    const MappedColumnVector& inY) {

    return (inX - inY).lpNorm<1>();
}

double
distNorm2(
    const MappedColumnVector& inX,
    const MappedColumnVector& inY) {

    return (inX - inY).norm();
}

double
squaredDistNorm2(
    const MappedColumnVector& inX,
    const MappedColumnVector& inY) {

    return (inX - inY).squaredNorm();
}

double
distAngle(
    const MappedColumnVector& inX,
    const MappedColumnVector& inY) {

	// Deal with the undefined case where one of the norm is zero
	// Angle is not defined. Just return \pi.
	double xnorm = inX.norm(), ynorm = inY.norm();
	if (xnorm < std::numeric_limits<double>::denorm_min()
		|| ynorm < std::numeric_limits<double>::denorm_min())
		return std::acos(-1);
	
    double cosine = dot(inX, inY) / (xnorm * ynorm);
    if (cosine > 1)
        cosine = 1;
    else if (cosine < -1)
        cosine = -1;
    return std::acos(cosine);
}

double
distTanimoto(
    const MappedColumnVector& inX,
    const MappedColumnVector& inY) {

    // Note that this is not a metric in general!
    double dotProduct = dot(inX, inY);
    double tanimoto = inX.squaredNorm() + inY.squaredNorm();
    return (tanimoto - 2 * dotProduct) / (tanimoto - dotProduct);
}

/**
 * @brief Compute the k columns of a matrix that are closest to a vector
 *
 * For performance, we cheat here: For the following four distance functions, we
 * take a special shortcut.
 * FIXME: FunctionHandle should be tuned so that this shortcut no longer
 * impacts performance by more than, say, ~10%.
 */
template <class RandomAccessIterator>
inline
void
closestColumnsAndDistancesShortcut(
    const MappedMatrix& inMatrix,
    const MappedColumnVector& inVector,
    FunctionHandle &inDist,
    RandomAccessIterator ioFirst,
    RandomAccessIterator ioLast) {

    // Sorted in the order of expected use
    if (inDist.funcPtr() == funcPtr<squared_dist_norm2>())
        closestColumnsAndDistances(inMatrix, inVector, squaredDistNorm2,
            ioFirst, ioLast);
    else if (inDist.funcPtr() == funcPtr<dist_norm2>())
        closestColumnsAndDistances(inMatrix, inVector, distNorm2,
            ioFirst, ioLast);
    else if (inDist.funcPtr() == funcPtr<dist_norm1>())
        closestColumnsAndDistances(inMatrix, inVector, distNorm1,
            ioFirst, ioLast);
    else if (inDist.funcPtr() == funcPtr<dist_angle>())
        closestColumnsAndDistances(inMatrix, inVector, distAngle,
            ioFirst, ioLast);
    else if (inDist.funcPtr() == funcPtr<dist_tanimoto>())
        closestColumnsAndDistances(inMatrix, inVector, distTanimoto,
            ioFirst, ioLast);
    else
        closestColumnsAndDistances(inMatrix, inVector, inDist,
            ioFirst, ioLast);
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
    closestColumnsAndDistancesShortcut(M, x, dist, &result, &result + 1);

    AnyType tuple;
    return tuple
        << static_cast<int32_t>(std::get<0>(result))
        << std::get<1>(result);
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
closest_columns::run(AnyType& args) {
    MappedMatrix M = args[0].getAs<MappedMatrix>();
    MappedColumnVector x = args[1].getAs<MappedColumnVector>();
    uint32_t num = args[2].getAs<uint32_t>();
    FunctionHandle dist = args[3].getAs<FunctionHandle>()
        .unsetFunctionCallOptions(FunctionHandle::GarbageCollectionAfterCall);

    std::vector<std::tuple<Index, double> > result(num);
    closestColumnsAndDistancesShortcut(M, x, dist, result.begin(),
        result.end());

    MutableArrayHandle<int32_t> indices = allocateArray<int32_t,
        dbal::FunctionContext, dbal::DoNotZero, dbal::ThrowBadAlloc>(num);
    MutableArrayHandle<double> distances = allocateArray<double,
        dbal::FunctionContext, dbal::DoNotZero, dbal::ThrowBadAlloc>(num);
    for (uint32_t i = 0; i < num; ++i)
        std::tie(indices[i], distances[i]) = result[i];

    AnyType tuple;
    return tuple << indices << distances;
}


AnyType
norm1::run(AnyType& args) {
    return static_cast<double>(args[0].getAs<MappedColumnVector>().lpNorm<1>());
}

AnyType
norm2::run(AnyType& args) {
    return static_cast<double>(args[0].getAs<MappedColumnVector>().norm());
}

AnyType
dist_norm1::run(AnyType& args) {
    return distNorm1(
        args[0].getAs<MappedColumnVector>(),
        args[1].getAs<MappedColumnVector>()
    );
}

AnyType
dist_norm2::run(AnyType& args) {
    // FIXME: it would be nice to declare this as a template function (so it
    // works for dense and sparse vectors), and the C++ AL takes care of the
    // rest...
    return distNorm2(
        args[0].getAs<MappedColumnVector>(),
        args[1].getAs<MappedColumnVector>()
    );
}

AnyType
squared_dist_norm2::run(AnyType& args) {
    return squaredDistNorm2(
        args[0].getAs<MappedColumnVector>(),
        args[1].getAs<MappedColumnVector>()
    );
}

AnyType
dist_angle::run(AnyType& args) {
    return distAngle(
        args[0].getAs<MappedColumnVector>(),
        args[1].getAs<MappedColumnVector>()
    );
}

AnyType
dist_tanimoto::run(AnyType& args) {
    return distTanimoto(
        args[0].getAs<MappedColumnVector>(),
        args[1].getAs<MappedColumnVector>()
    );
}

} // namespace linalg

} // namespace modules

} // namespace regress
