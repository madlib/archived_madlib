/* ----------------------------------------------------------------------- *//**
 *
 * @file matrix_agg.cpp
 *
 * @brief Build a matrix using the given column vectors
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <utils/Math.hpp>

#include "matrix_agg.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

namespace linalg {

/**
 * @brief Transition state for building a matrix
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 3, and all elemenets are 0. Handle::operator[] will
 * perform bounds checking.
 */
template <class Handle>
class MatrixAggState {
    template <class OtherHandle>
    friend class MatrixAggState;

public:
    MatrixAggState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint64_t>(mStorage[0]),
            static_cast<uint64_t>(mStorage[1]));
    }

    operator AnyType() const {
        return mStorage;
    }

    void initialize(uint64_t inNumRows) {
        rebind(inNumRows, 0);
        numRows = inNumRows;
    }

    typename HandleTraits<Handle>::MatrixTransparentHandleMap::ColXpr
    newColumn(const Allocator& inAllocator) {
        uint64_t numColsReserved = utils::nextPowerOfTwo(
            static_cast<uint16_t>(numCols));

        if (numColsReserved <= numCols) {
            if (numColsReserved == 0)
                numColsReserved = 1;
            else
                numColsReserved *= 2;

            MatrixAggState oldSelf = *this;
            mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(
                    arraySize(numRows, numColsReserved));
            rebind(numRows, numColsReserved);
            numRows = oldSelf.numRows;
            numCols = oldSelf.numCols;
            matrix.leftCols(numCols) = oldSelf.matrix.leftCols(numCols);
        }

        rebind(numRows, numCols + 1);
        return matrix.col(numCols++);
    }

private:
    static inline uint64_t arraySize(uint64_t inNumRows, uint64_t inNumCols) {
        return 2ULL + inNumRows * inNumCols;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inNumRows The number of rows
     * @param inNumCols The number of columns
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: numRows (number of rows)
     * - 1: numCols (number of columns)
     * - 2: sumOfPoints (matrix with \c numRows rows and \c numCols columns)
     */
    void rebind(uint64_t inNumRows, uint64_t inNumCols) {
        numRows.rebind(&mStorage[0]);
        numCols.rebind(&mStorage[1]);
        matrix.rebind(&mStorage[2], inNumRows, inNumCols);

        madlib_assert(mStorage.size()
            >= arraySize(inNumRows, inNumCols),
            std::runtime_error("Out-of-bounds array access detected."));
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt64 numCols;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap matrix;
};


AnyType
matrix_agg_transition::run(AnyType& args) {
    MatrixAggState<MutableArrayHandle<double> > state = args[0];
    MappedColumnVector x = args[1].getAs<MappedColumnVector>();

    if (state.numCols == 0)
        state.initialize(x.size());
    else if (x.size() != state.matrix.rows()
        || state.numRows != static_cast<uint64_t>(state.matrix.rows())
        || state.numCols != static_cast<uint64_t>(state.matrix.cols()))
        throw std::invalid_argument("Invalid arguments: Dimensions of vectors "
            "not consistent.");

    state.newColumn(*this) = x;
    return state;
}

AnyType
matrix_agg_final::run(AnyType& args) {
    MatrixAggState<ArrayHandle<double> > state = args[0];

    return MappedMatrix(state.matrix.leftCols(state.numCols));
}

AnyType
matrix_column::run(AnyType& args) {
    MappedMatrix M = args[0].getAs<MappedMatrix>();
    uint32_t column = args[1].getAs<uint32_t>();

    return MappedColumnVector(M.col(column));
}

} // namespace linalg

} // namespace modules

} // namespace regress
