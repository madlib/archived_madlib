/* ----------------------------------------------------------------------- *//**
 *
 * @file dense_linear_systems.cpp
 *
 * @brief Dense linear systems
 *
 * We implement dense linear systems using the direct.
 *
 *//* ----------------------------------------------------------------------- */
#include <limits>
#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>
#include "dense_linear_systems.hpp"

// Common SQL functions used by all modules
#include "dense_linear_systems_states.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

namespace linear_systems{

// Residual Computation
// ---------------------------------------------------------------------------
typedef ResidualState<RootContainer> IResidualState;
typedef ResidualState<MutableRootContainer> MutableResidualState;

/**
 * @brief Residual computation transition step
 */
AnyType dense_residual_norm_transition::run(AnyType& args){

    MutableResidualState state = args[0].getAs<MutableByteString>();
    MappedColumnVector _a = args[1].getAs<MappedColumnVector>();
    double _b = args[2].getAs<double>();
    MappedColumnVector x = args[3].getAs<MappedColumnVector>();

    state.numRows++;
    double x_a = dot(x,_a);

    // Avoiding 2 norm for overflow reasons
    state.residual_norm += std::abs(x_a - _b);
    state.b_norm += std::abs(_b);

    return state.storage();
}


/**
 * @brief Perform the preliminary aggregation function: Merge transition states
 */
AnyType dense_residual_norm_merge_states::run(AnyType& args)
{
    MutableResidualState state1 = args[0].getAs<MutableByteString>();
    IResidualState state2 = args[1].getAs<ByteString>();

    if (state1.numRows == 0)
        return state2.storage();
    else if (state2.numRows == 0)
        return state1.storage();

    state1.numRows += state2.numRows;
    state1.residual_norm += state2.residual_norm;
    state1.b_norm += state2.b_norm;
    return state1.storage();
}

/**
 * @brief Final step of the residual computations
 */
AnyType dense_residual_norm_final::run(AnyType& args)
{
    IResidualState state = args[0].getAs<ByteString>();

    if (state.numRows == 0)
      return Null();

    AnyType tuple;
    // Return scaled norm
    tuple << state.residual_norm / state.b_norm;
    return tuple;
}


// ---------------------------------------------------------------------------
//              Direct Dense Linear System States
// ---------------------------------------------------------------------------

// Function protofype of Internal functions
AnyType direct_dense_stateToResult(
    const Allocator &inAllocator,
    const ColumnVector &x);

/**
 * @brief States for linear systems
 *
 * TransitionState encapsualtes the transition state during the
 * logistic-regression aggregate function. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elemenets are 0.
 *
 */
template <class Handle>
class DenseDirectLinearSystemTransitionState {
    template <class OtherHandle>
    friend class DenseDirectLinearSystemTransitionState;

public:
    DenseDirectLinearSystemTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind( static_cast<uint32_t>(mStorage[0]),
                static_cast<uint32_t>(mStorage[1]));
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
                          uint32_t inWidthOfA,
                          uint32_t inWidthOfb
                          ) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfA, inWidthOfb));
        rebind(inWidthOfA, inWidthOfb);
        widthOfA = inWidthOfA;
        widthOfb = inWidthOfb;
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    DenseDirectLinearSystemTransitionState &operator=(
        const DenseDirectLinearSystemTransitionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    DenseDirectLinearSystemTransitionState &operator+=(
        const DenseDirectLinearSystemTransitionState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfA != inOtherState.widthOfA ||
            widthOfb != inOtherState.widthOfb)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");

        numRows += inOtherState.numRows;
        A += inOtherState.A;
        b += inOtherState.b;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        algorithm = 0;
        A.fill(0);
        b.fill(0);
    }

private:
    static inline size_t arraySize(const uint32_t inWidthOfA,
                                   const uint32_t inWidthOfb) {
        return 4 + 1 * inWidthOfb * inWidthOfA + 1 * inWidthOfb;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfA The number of independent variables.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: widthOfA (number of variables)
     * - 1: widthOfb (number of equations)
     * - 2: numRows
     * - 3: algorithm
     * - 4: b (RHS vector)
     * - 4 + 1 * widthOfb: a (LHS matrix)
     */
    void rebind(uint32_t inWidthOfA, uint32_t inWidthOfb) {
        widthOfA.rebind(&mStorage[0]);
        widthOfb.rebind(&mStorage[1]);
        numRows.rebind(&mStorage[2]);
        algorithm.rebind(&mStorage[3]);
        b.rebind(&mStorage[4], inWidthOfb);
        A.rebind(&mStorage[4 + 1 * inWidthOfb], inWidthOfb, inWidthOfA);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt32 widthOfA;
    typename HandleTraits<Handle>::ReferenceToUInt32 widthOfb;
    typename HandleTraits<Handle>::ReferenceToUInt32 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt32 algorithm;

    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap b;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap A;
};


/**
 * @brief Perform the dense linear system transition step
 */
AnyType
dense_direct_linear_system_transition::run(AnyType &args) {

    DenseDirectLinearSystemTransitionState<MutableArrayHandle<double> >
                                                              state = args[0];
    int32_t row_id = args[1].getAs<int32_t>();
    MappedColumnVector _a = args[2].getAs<MappedColumnVector>();
    double _b = args[3].getAs<double>();
    int32_t num_equations = args[4].getAs<int32_t>();


    // The following check was added with MADLIB-138.
    if (!dbal::eigen_integration::isfinite(_a)) {
        throw std::domain_error("Input matrix is not finite.");
        return state;
    }

    if (state.numRows == 0) {

        int algorithm = args[5].getAs<int>();
        state.initialize(*this,
                        static_cast<uint32_t>(_a.size()),
                        num_equations
                        );
        state.algorithm = algorithm;
    }

    state.numRows++;

	// Now do the transition step
	// First we create a block of zeros in memory
	// and then add the vector in the appropriate position
    ColumnVector bvec(static_cast<uint32_t>(state.widthOfb));
    Matrix Amat(static_cast<uint32_t>(state.widthOfb),
                static_cast<uint32_t>(state.widthOfA));
    bvec.setZero();
    Amat.setZero();
    bvec(row_id) = _b;
    Amat.row(row_id) = _a;

    // Build the vector & matrices based on row_id
    state.A += Amat;
    state.b += bvec;
    return state;
}


/**
 * @brief Perform the preliminary aggregation function: Merge transition states
 */
AnyType
dense_direct_linear_system_merge_states::run(AnyType &args) {
    DenseDirectLinearSystemTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    DenseDirectLinearSystemTransitionState<ArrayHandle<double> > stateRight = args[1];

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


/**
 * @brief Perform the linear system final step
 */
AnyType
dense_direct_linear_system_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    DenseDirectLinearSystemTransitionState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    ColumnVector x;

    switch (state.algorithm) {
      case 1: x = (state.A).partialPivLu().solve(state.b);
              break;
      case 2: x = (state.A).fullPivLu().solve(state.b);
              break;
      case 3: x = (state.A).householderQr().solve(state.b);
              break;
      case 4: x = (state.A).colPivHouseholderQr().solve(state.b);
              break;
      case 5: x = (state.A).fullPivHouseholderQr().solve(state.b);
              break;
      case 6: x = (state.A).llt().solve(state.b);
              break;
      case 7: x = (state.A).ldlt().solve(state.b);
              break;
    }

    // Compute the residual
    return direct_dense_stateToResult(*this, x);
}

/**
 * @brief Helper function that computes the final statistics for the robust variance
 */

AnyType direct_dense_stateToResult(
    const Allocator &inAllocator,
	  const ColumnVector &x) {

	MutableNativeColumnVector solution(
        inAllocator.allocateArray<double>(x.size()));

    for (Index i = 0; i < x.size(); ++i) {
        solution(i) = x(i);
    }

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << solution
          << Null()
          << Null();

    return tuple;
}


} // namespace linear_systems

} // namespace modules

} // namespace madlib
