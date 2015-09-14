/* ----------------------------------------------------------------------- *//**
 *
 * @file matrix_decomp.cpp
 *
 * @brief Compute decomposition of a matrix in a single node
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <utils/Math.hpp>
#include <Eigen/Core>
#include <Eigen/LU>

#include "matrix_decomp.hpp"

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
class MatrixComposeState {
    template <class OtherHandle>
    friend class MatrixComposeState;

public:
    MatrixComposeState(const AnyType &inArray)
            : mStorage(inArray.getAs<Handle>()) {
        rebind(static_cast<uint64_t>(mStorage[0]), static_cast<uint64_t>(mStorage[1]));
    }

    operator AnyType() const {
        return mStorage;
    }

    void initialize(const Allocator& inAllocator, uint64_t inNumRows, uint64_t inNumCols) {
        // Allocate the storage for the matrix
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(stateSize(inNumRows, inNumCols));
        rebind(inNumRows, inNumCols);
        numRows = inNumRows;
        numCols = inNumCols;
        matrix.fill(0);
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    MatrixComposeState &operator=(
        const MatrixComposeState<OtherHandle> &inOtherState) {
        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object
     */
    template <class OtherHandle>
    MatrixComposeState &operator+=(
        const MatrixComposeState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            numRows != inOtherState.numRows ||
            numCols != inOtherState.numCols)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");
        // we assume that the number of rows and number of cols remain the same
        // between the merged states. We only need to add the matrices together
        // since each row/element of the matrix is set only once.
        matrix += inOtherState.matrix;
        return *this;
    }

private:
    static inline size_t stateSize(uint64_t inNumRows, uint64_t inNumCols) {
        return static_cast<size_t>(2 + inNumRows * inNumCols);
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
        matrix.rebind(&mStorage[2], static_cast<Index>(inNumRows),
                      static_cast<Index>(inNumCols));

        madlib_assert(mStorage.size()
            >= stateSize(inNumRows, inNumCols),
            std::runtime_error("Out-of-bounds array access detected."));
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt64 numCols;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap matrix;
};


AnyType
matrix_compose_dense_transition::run(AnyType& args) {
    MatrixComposeState<MutableArrayHandle<double> > state = args[0];
    uint32_t numRows = args[1].getAs<uint32_t>();
    Index row_id = args[2].getAs<uint32_t>();
    MappedColumnVector curr_row = args[3].getAs<MappedColumnVector>();

    if (state.numCols == 0)
        state.initialize(*this, numRows, static_cast<uint32_t>(curr_row.size()));
    else if (curr_row.size() != state.matrix.cols() ||
             state.numRows != static_cast<uint32_t>(state.matrix.rows()) ||
             state.numCols != static_cast<uint32_t>(state.matrix.cols()))
        throw std::invalid_argument("Invalid arguments: Dimensions of vectors "
                                    "not consistent.");
    if (row_id < 0 || row_id >= numRows)
            throw std::runtime_error("Invalid row id.");
    state.matrix.row(row_id) = curr_row;
    return state;
}

AnyType
matrix_compose_sparse_transition::run(AnyType& args) {
    MatrixComposeState<MutableArrayHandle<double> > state = args[0];
    uint32_t numRows = args[1].getAs<uint32_t>();
    uint32_t numCols = args[2].getAs<uint32_t>();
    Index row_id = args[3].getAs<uint32_t>();
    Index col_id = args[4].getAs<uint32_t>();
    double element = args[5].getAs<double>();

    if (state.numCols == 0)
        state.initialize(*this, numRows, numCols);
    else if (state.numRows != static_cast<uint32_t>(state.matrix.rows()) ||
             state.numCols != static_cast<uint32_t>(state.matrix.cols()))
        throw std::invalid_argument("Invalid arguments: Dimensions of vectors "
                                    "not consistent.");
    if (row_id < 0 || row_id >= numRows)
            throw std::runtime_error("Invalid row id.");
    if (col_id < 0 || col_id >= numCols)
            throw std::runtime_error("Invalid col id.");
    state.matrix(row_id, col_id) = element;
    return state;
}


/**
 * @brief Perform the preliminary aggregation function: Merge transition states
 */
AnyType
matrix_compose_merge::run(AnyType &args) {
    if (args[0].isNull()) { return args[1]; }
    if (args[1].isNull()) { return args[0]; }
    MatrixComposeState<MutableArrayHandle<double> > stateLeft = args[0];
    MatrixComposeState<ArrayHandle<double> > stateRight = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.numRows == 0)
        return stateRight;
    else if (stateRight.numRows == 0)
        return stateLeft;

    // Merge states together and return
    stateLeft += stateRight;
    return stateLeft;
}

AnyType
matrix_inv::run(AnyType& args){
    if (args.isNull()) {
        return Null();
    }
    MatrixComposeState<ArrayHandle<double> > state = args[0];
    // we apply a transpose operation at the end since Eigen matrices are interpreted
    // as column-major when returned to the database
    const Matrix m_inverse = state.matrix.inverse().transpose();
    return m_inverse;
}

AnyType
matrix_eigen::run(AnyType& args) {
    if (args.isNull()) {
        return Null();
    }
    MatrixComposeState<ArrayHandle<double> > state = args[0];
    // we apply a transpose operation at the end since Eigen matrices are interpreted
    // as column-major when returned to the database
    const VectorXcd eigenvalues = state.matrix.eigenvalues();
    return MappedVectorXcd(eigenvalues.leftCols(static_cast<Index>(1)));
}

AnyType
matrix_cholesky::run(AnyType& args){
    if (args.isNull()) {
        return Null();
    }
    MatrixComposeState<ArrayHandle<double> > state = args[0];
    const MatrixLDLT ldlt = state.matrix.ldlt();
    if (ldlt.info() != Success) {
        throw std::invalid_argument("Invalida arguments: Cholesky decomposition of input matrix"
                " does not exist");
    }
    // we apply a transpose operation at the end since Eigen matrices are interpreted
    // as column-major when returned to the database
    const Matrix m_p(PermutationMatrix(ldlt.transpositionsP()));
    const Matrix m_l = ldlt.matrixL();
    const Matrix m_d(ldlt.vectorD().asDiagonal());
    Matrix m_cholesky(state.matrix.rows(), state.matrix.cols() * 3);
    m_cholesky.block(0, 0, state.matrix.rows(), state.matrix.cols()) << m_p;
    m_cholesky.block(0, state.matrix.cols(), state.matrix.rows(), state.matrix.cols()) << m_l;
    m_cholesky.block(0, state.matrix.cols() * 2, state.matrix.rows(), state.matrix.cols()) << m_d;
    const Matrix res = m_cholesky.transpose();
    return res;
}

AnyType
matrix_qr::run(AnyType& args){
    if (args.isNull()) {
        return Null();
    }
    MatrixComposeState<ArrayHandle<double> > state = args[0];
    const HouseholderQR qr = state.matrix.householderQr();
    const Matrix &R = qr.matrixQR().triangularView<Upper>();
    const Matrix &Q =  qr.householderQ();

    madlib_assert(Q.rows() == Q.cols() && Q.cols() == R.rows(),
        std::runtime_error("Error QR decomposition result."));
    Matrix m(Q.rows(), Q.cols() + R.cols());
    m.block(0, 0, Q.rows(), Q.cols()) << Q;
    m.block(0, Q.cols(), R.rows(), R.cols()) << R;
    // we apply a transpose operation at the end since Eigen matrices are interpreted
    // as column-major when returned to the database
    const Matrix & res = m.transpose();
    return res;
}

AnyType
matrix_rank::run(AnyType& args){
    if (args.isNull()) {
        return Null();
    }
    MatrixComposeState<ArrayHandle<double> > state = args[0];
    return static_cast<int64_t>(state.matrix.fullPivLu().rank());
}

AnyType
matrix_lu::run(AnyType& args){
    if (args.isNull()) {
        return Null();
    }
    MatrixComposeState<ArrayHandle<double> > state = args[0];
    const FullPivLU &lu = state.matrix.fullPivLu();

    Matrix l = Matrix::Identity(state.numRows, state.numRows);
    l.block(0, 0, lu.matrixLU().rows(), lu.matrixLU().cols()).triangularView<StrictlyLower>() = lu.matrixLU();
    const Matrix &u = lu.matrixLU().triangularView<Upper>();
    const Matrix &p(lu.permutationP());
    const Matrix &q(lu.permutationQ());
    Matrix m(static_cast<Index>(std::max(state.numRows, state.numCols)),
             static_cast<Index>(state.numRows * 2 + state.numCols * 2));
    m.block(0, 0, state.numRows, state.numRows) << p;
    m.block(0, state.numRows, state.numRows, state.numRows) << l;
    m.block(0, state.numRows * 2, state.numRows, state.numCols) << u;
    m.block(0, state.numRows * 2 + state.numCols, state.numCols, state.numCols) << q;

    // we apply a transpose operation at the end since Eigen matrices are interpreted
    // as column-major when returned to the database
    const Matrix res = m.transpose();
    return res;
}

AnyType
matrix_nuclear_norm::run(AnyType& args){
    if (args.isNull()) {
        return Null();
    }
    MatrixComposeState<ArrayHandle<double> > state = args[0];
    const JacobiSVD &svd = state.matrix.jacobiSvd(EigenvaluesOnly);
    const Matrix u = svd.matrixU();
    const Matrix v = svd.matrixV();
    double norm = 0;
    for (Index i = 0; i < svd.singularValues().rows(); ++i)
        norm += svd.singularValues()(i);
    return norm;
}

AnyType
matrix_pinv::run(AnyType& args) {
    if (args.isNull()) {
        return Null();
    }
    using namespace std;
    MatrixComposeState<ArrayHandle<double> > state = args[0];
    const JacobiSVD &svd = state.matrix.jacobiSvd(ComputeFullU|ComputeFullV);
    const Matrix u = svd.matrixU();
    const Matrix v = svd.matrixV();
    Matrix s(svd.singularValues().asDiagonal());
    double pinvtoler = 1.e-6;
    for (Index i = 0; i < s.rows(); ++i) {
        for (Index j = 0; j < s.cols(); ++j) {
            // for very small singular values we treat the inverse as zero
            if (s(i, j) > pinvtoler)
                s(i, j) = 1.0 / s(i, j);
            else s(i, j) = 0;
        }
    }
    // we apply a transpose operation at the end since Eigen matrices are interpreted
    // as column-major when returned to the database
    const Matrix &res = (v * s * u.transpose()).transpose();
    return res;
}

} // namespace linalg

} // namespace modules

} // namespace regress
