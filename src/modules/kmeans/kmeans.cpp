/* ----------------------------------------------------------------------- *//** 
 *
 * @file kmeans.cpp
 *
 * @brief k-Means
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/linalg/linalg.hpp>
#include <modules/shared/HandleTraits.hpp>

#include "kmeans.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

namespace kmeans {

template <class Handle>
class KMeansState {
    template <class OtherHandle>
    friend class KMeansState;

public:
    KMeansState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {
        
        rebind(static_cast<uint16_t>(mStorage[1]),
            static_cast<uint16_t>(mStorage[2]));
    }
    
    /**
     * @brief Convert to backend representation
     *
     * We define this function so that we can use State in the
     * argument list and as a return type.
     */
    inline operator AnyType() const {
        return mStorage;
    }
    
    /**
     * @brief Initialize the conjugate-gradient state.
     * 
     * This function is only called for the first iteration, for the first row.
     */
    inline void initialize(const Allocator &inAllocator,
        uint16_t inNumDimensions, uint16_t inNumCentroids) {
        
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(
                arraySize(inNumDimensions, inNumCentroids));
        rebind(inNumDimensions, inNumCentroids);
        numDimensions = inNumDimensions;
        numCentroids = inNumCentroids;
    }
    
    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    KMeansState &operator+=(
        const KMeansState<OtherHandle> &inOtherState) {
        
        if (mStorage.size() != inOtherState.mStorage.size() ||
            numDimensions != inOtherState.numDimensions ||
            numCentroids != inOtherState.numCentroids)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");
        
        numRows += inOtherState.numRows;
        numReassigned += inOtherState.numReassigned;
        sumOfClosestPoints += inOtherState.sumOfClosestPoints;
        numClosestPoints += inOtherState.numClosestPoints;
        return *this;
    }
    
private:
    static inline uint64_t arraySize(uint16_t inNumDimensions,
        uint16_t inNumCentroids) {
        
        return 4 + inNumDimensions * inNumCentroids + inNumCentroids;
    }
    
    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX The number of independent variables.
     * @param 
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: numRows (number of rows already processed in this iteration)
     * - 1: numDimensions (dimension of space that points are from)
     * - 2: numCentroids (number of centroids)
     * - 3: numReassigned (number of rows processed so far where closest
     *   centroid differs from previous iteration)
     * - 4: sumOfClosestPoints (matrix with \c numDimensions rows and \c
     *   \c numCentroids columns, each column contains the sum of all points
     *   that are closest to this column)
     * - 4 + numDimensions * numCentroids: numClosestPoints (for each centroid,
     *   number of points that are closest to this centroid)
     */
    void rebind(uint16_t inNumDimensions, uint16_t inNumCentroids) {
        numRows.rebind(&mStorage[0]);
        numDimensions.rebind(&mStorage[1]);
        numCentroids.rebind(&mStorage[2]);
        numReassigned.rebind(&mStorage[3]);
        sumOfClosestPoints.rebind(&mStorage[4], inNumDimensions, inNumCentroids);
        numClosestPoints.rebind(&mStorage[4 + inNumDimensions * inNumCentroids],
            inNumCentroids);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt16 numDimensions;
    typename HandleTraits<Handle>::ReferenceToUInt16 numCentroids;
    typename HandleTraits<Handle>::ReferenceToUInt64 numReassigned;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap sumOfClosestPoints;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap numClosestPoints;
};



AnyType
kmeans_step_transition::run(AnyType& args) {
    KMeansState<MutableArrayHandle<double> > state = args[0];
    MappedColumnVector x = args[1].getAs<MappedColumnVector>();
    MappedMatrix centroids = args[2].getAs<MappedMatrix>();
    AnyType previousCentroidsArg = args[3];
    FunctionHandle dist = args[4].getAs<FunctionHandle>();
    
    if (state.numRows == 0)
        state.initialize(*this, centroids.rows(), centroids.cols());
    else if (x.size() != centroids.rows()
        || state.numDimensions != centroids.rows()
        || state.numCentroids != centroids.cols())
        throw std::invalid_argument("Invalid arguments: Dimensions of points "
            "not consistent.");

    Index closestColumn = std::get<0>(
        linalg::closestColumnAndDistance(centroids, x, dist) );
    
    ++state.numRows;
    state.sumOfClosestPoints.col(closestColumn) += x;
    ++state.numClosestPoints(closestColumn);
    if (previousCentroidsArg.isNull()) {
        ++state.numReassigned;
    } else {
        MappedMatrix previousCentroids
            = previousCentroidsArg.getAs<MappedMatrix>();
        Index previousClosestColumn = std::get<0>(
            linalg::closestColumnAndDistance(previousCentroids, x, dist) );

        if (previousClosestColumn != closestColumn)
            ++state.numReassigned;        
    }
        
    return state;
}

AnyType
kmeans_step_merge::run(AnyType& args) {
    KMeansState<MutableArrayHandle<double> > stateLeft = args[0];
    KMeansState<ArrayHandle<double> > stateRight = args[1];
    
    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.numRows == 0)
        return stateRight;
    else if (stateRight.numRows == 0)
        return stateLeft;
    
    stateLeft += stateRight;
    return stateLeft;
}

AnyType
kmeans_step_final::run(AnyType& args) {
    KMeansState<ArrayHandle<double> > state = args[0];

    int16_t numNonIsolatedCentroids = 0;
    for (Index i = 0; i < state.sumOfClosestPoints.cols(); ++i) {
        if (state.numClosestPoints(i) > 0)
            ++numNonIsolatedCentroids;
    }

    // Note that the dimensions for array allocations are in reverse order
    // (compared to the matrix notation). We want to store an array of columns:
    // So the first dimension is the number of columns and the second dimension
    // is the number of rows (i.e., the individual elements of a column).
    // As is common in scientific computing (LAPACK, Eigen), we use
    // column-oriented storage for dense matrices (i.e., we want columns to be
    // contiguous blocks in memory).
    MutableMappedMatrix newCentroids(allocateArray<double>(
        numNonIsolatedCentroids, state.sumOfClosestPoints.rows()));
    
    numNonIsolatedCentroids = 0;
    for (Index i = 0; i < state.sumOfClosestPoints.cols(); ++i) {
        if (state.numClosestPoints(i) > 0) {
            newCentroids.col(numNonIsolatedCentroids)
                = state.sumOfClosestPoints.col(i) / state.numClosestPoints(i);
            ++numNonIsolatedCentroids;
        }
    }
    
    AnyType tuple;
    return tuple << newCentroids <<
        static_cast<double>(state.numReassigned) / state.numRows;
}

} // namespace kmeans

} // namespace modules

} // namespace regress
