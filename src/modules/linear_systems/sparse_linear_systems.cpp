/* ----------------------------------------------------------------------- *//**
 *
 * @file sparse_linear_systems.cpp
 *
 * @brief sparse linear systems
 *
 * We implement sparse linear systems using the direct. 
 *
 *//* ----------------------------------------------------------------------- */
#include <limits>
#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>
#include "sparse_linear_systems.hpp"
#include <Eigen/Sparse>

// Common SQL functions used by all modules
//#include "sparse_linear_systems_states.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;
using namespace Eigen;

namespace modules {

// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

namespace linear_systems{


// ---------------------------------------------------------------------------
//              Direct sparse Linear System States
// ---------------------------------------------------------------------------

// Function protofype of Internal functions
AnyType direct_sparse_stateToResult(
    const Allocator &inAllocator,
    const ColumnVector &x,
    const double residual);


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
class SparseDirectLinearSystemTransitionState {
    template <class OtherHandle>
    friend class SparseDirectLinearSystemTransitionState;

public:
    SparseDirectLinearSystemTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind( static_cast<uint32_t>(mStorage[1]),
                static_cast<uint32_t>(mStorage[2]));
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
                          uint32_t innumVars,
                          uint32_t innumEquations,
                          uint32_t inNNZA
                          ) {
        // Array size does not depend in numVars
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(innumEquations, inNNZA));
        rebind(innumEquations, inNNZA);
        numVars = innumVars;
        numEquations = innumEquations;
        NNZA = inNNZA;
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    SparseDirectLinearSystemTransitionState &operator=(
        const SparseDirectLinearSystemTransitionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    SparseDirectLinearSystemTransitionState &operator+=(
        const SparseDirectLinearSystemTransitionState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            numVars != inOtherState.numVars || 
            NNZA != inOtherState.NNZA|| 
            numEquations != inOtherState.numEquations)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");

        b += inOtherState.b;
        b_stored += inOtherState.b_stored;
        
        // Merge state for sparse loading is not an array add operation
        // but it is an array-append operation
        for (uint32_t i=0; i < inOtherState.nnz_processed; i++){
            r(nnz_processed + i) += inOtherState.r(i);
            c(nnz_processed + i) += inOtherState.c(i);
            v(nnz_processed + i) += inOtherState.v(i);
        }
        nnz_processed += inOtherState.nnz_processed;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        nnz_processed = 0;
        r.fill(0);
        c.fill(0);
        v.fill(0);
        b.fill(0);
        b_stored.fill(0);
    }

private:
    static inline size_t arraySize(const uint32_t innumEquations,
                                   const uint32_t inNNZA) {
        return 5 + 3 * inNNZA + 2 * innumEquations;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param innumVars The number of independent variables.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: numVars                (Total number of variables)
     * - 1: numEquations           (Total number of equations)
     * - 2: nnZ                    (Total number of non-zeros)
     * - 3: algorithm
     * - 4: nnz_processed          Number of non-zeros processed by a node
     * - 4: b (RHS vector)
     * - 4 + 1 * numEquations: r (LHS matrix row)
     * - 4 + 1 * numEquations + 1 * nnzA: c (LHS matrix column)
     * - 4 + 1 * numEquations + 2 * nnzA: v (LHS matrix value)
     */
    void rebind(uint32_t innumEquations, uint32_t inNNZA) {
        numVars.rebind(&mStorage[0]);
        numEquations.rebind(&mStorage[1]);
        NNZA.rebind(&mStorage[2]);
        algorithm.rebind(&mStorage[3]);
        nnz_processed.rebind(&mStorage[4]);
        b.rebind(&mStorage[5], innumEquations);
        b_stored.rebind(&mStorage[5 + innumEquations], innumEquations);
        r.rebind(&mStorage[5 + 2*innumEquations], inNNZA);
        c.rebind(&mStorage[5 + 2*innumEquations + inNNZA], inNNZA);
        v.rebind(&mStorage[5 + 2*innumEquations + 2 * inNNZA], inNNZA);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt32 numVars;
    typename HandleTraits<Handle>::ReferenceToUInt32 numEquations;
    typename HandleTraits<Handle>::ReferenceToUInt32 NNZA;
    typename HandleTraits<Handle>::ReferenceToUInt32 nnz_processed;
    typename HandleTraits<Handle>::ReferenceToUInt32 algorithm;

    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap b_stored;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap b;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap r;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap c;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap v;
};


/**
 * @brief Perform the sparse linear system transition step
 */
AnyType
sparse_direct_linear_system_transition::run(AnyType &args) {

    SparseDirectLinearSystemTransitionState<MutableArrayHandle<double> > 
                                                              state = args[0];
    int32_t row_id = args[1].getAs<int32_t>();
    int32_t col_id = args[2].getAs<int32_t>();
    double value = args[3].getAs<double>();
    double _b = args[4].getAs<double>();


    
    // When the segment recieves the first non-zero in the sparse matrix
    // we initialize the state
    if (state.nnz_processed == 0) {
        
        int32_t num_equations = args[5].getAs<int32_t>();
        int32_t num_vars = args[6].getAs<int32_t>();
        int32_t total_nnz = args[7].getAs<int32_t>();
        int algorithm = args[8].getAs<int>();
        state.initialize(*this, 
                        num_vars,
                        num_equations,
                        total_nnz
                        );
        state.algorithm = algorithm;
        state.NNZA = total_nnz;
        state.numVars = num_vars;
        state.numEquations = num_equations;
        state.b_stored.setZero();
        state.b.setZero();

    }

	// Now do the transition step
	// First we create a block of zeros in memory
	// and then add the vector in the appropriate position
    ColumnVector bvec(static_cast<uint32_t>(state.numEquations));
    ColumnVector rvec(static_cast<uint32_t>(state.NNZA));
    ColumnVector cvec(static_cast<uint32_t>(state.NNZA));
    ColumnVector vvec(static_cast<uint32_t>(state.NNZA));
    
    bvec.setZero();
    rvec.setZero();
    cvec.setZero();
    vvec.setZero();

    rvec(state.nnz_processed) = row_id;
    cvec(state.nnz_processed) = col_id;
    vvec(state.nnz_processed) = value;

    if (state.b_stored(row_id) == 0) {
        bvec(row_id) = _b;
        state.b_stored(row_id) = 1; 
        state.b += bvec;
    }

    // Build the vector & matrices based on row_id
    state.r += rvec;
    state.c += cvec;
    state.v += vvec;
    state.nnz_processed++;

    return state;
}


/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
sparse_direct_linear_system_merge_states::run(AnyType &args) {
    SparseDirectLinearSystemTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    SparseDirectLinearSystemTransitionState<ArrayHandle<double> > stateRight = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.numEquations == 0)
        return stateRight;
    else if (stateRight.numEquations == 0)
        return stateLeft;

    // Merge states together and return
    stateLeft += stateRight;
    return stateLeft;
}



/**
 * @brief Perform the linear system final step
 */
AnyType
sparse_direct_linear_system_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    SparseDirectLinearSystemTransitionState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.numEquations == 0)
        return Null();
    
    // Eigen works better when you reserve the number of nnz
    // Note: When calling reserve(), it is not required that nnz is the 
    //       exact number of nonzero elements in the final matrix. However, an 
    //       exact estimation will avoid multiple reallocations during the 
    //       insertion phase.
    SparseMatrix A(state.numEquations, state.numVars);
    A.reserve(state.NNZA);
    ColumnVector x;

    for (uint32_t i=0; i < state.NNZA; i++){
        A.insert(static_cast<uint32_t>(state.r(i)),
                 static_cast<uint32_t>(state.c(i))) = state.v(i);
    }
    
    // Switch case needs scoping in C++ if you want to declare inside it
    // Unfortunately, this means that I have to write the code to call the 
    // solver in each switch case separtely
    switch (state.algorithm) {
      case 1:{
              Eigen::SimplicialLLT<SparseMatrix> solver;
              solver.compute(A);
              x = solver.solve(state.b);
              break;
      }
      case 2:{
              Eigen::SimplicialLDLT<SparseMatrix> solver;
              solver.compute(A);
              x = solver.solve(state.b);
              break;
      }
    }


    // It is unclear whether I should do the residual computation in-memory or 
    // in-database (as done in the dense linear systems case)
    ColumnVector bvec = state.b;
    double residual = (A*x-bvec).norm() / bvec.norm();

    // Compute the residual
    return direct_sparse_stateToResult(*this, x, residual);
}

/**
 * @brief Helper function that computes the final statistics 
 */

AnyType direct_sparse_stateToResult(
    const Allocator &inAllocator,
	const ColumnVector &x, 
	const double residual) {

	MutableNativeColumnVector solution(
        inAllocator.allocateArray<double>(x.size()));

    for (Index i = 0; i < x.size(); ++i) {
        solution(i) = x(i);
    }

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << solution 
          << residual
          << Null();

    return tuple;
}


// ---------------------------------------------------------------------------
//              In-memory iterative sparse Linear System States
// ---------------------------------------------------------------------------

// Function protofype of Internal functions
AnyType inmem_iterative_sparse_stateToResult(
    const Allocator &inAllocator,
    const ColumnVector &x,
	const int iters,
	const double residual_norm);


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
class SparseInMemIterativeLinearSystemTransitionState {
    template <class OtherHandle>
    friend class SparseInMemIterativeLinearSystemTransitionState;

public:
    SparseInMemIterativeLinearSystemTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind( static_cast<uint32_t>(mStorage[1]),
                static_cast<uint32_t>(mStorage[2]));
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
                          uint32_t innumVars,
                          uint32_t innumEquations,
                          uint32_t inNNZA
                          ) {
        // Array size does not depend in numVars
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(innumEquations, inNNZA));
        rebind(innumEquations, inNNZA);
        numVars = innumVars;
        numEquations = innumEquations;
        NNZA = inNNZA;
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    SparseInMemIterativeLinearSystemTransitionState &operator=(
        const SparseInMemIterativeLinearSystemTransitionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    SparseInMemIterativeLinearSystemTransitionState &operator+=(
        const SparseInMemIterativeLinearSystemTransitionState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            numVars != inOtherState.numVars || 
            NNZA != inOtherState.NNZA|| 
            numEquations != inOtherState.numEquations)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");

        b += inOtherState.b;
        b_stored += inOtherState.b_stored;
        
        // Merge state for sparse loading is not an array add operation
        // but it is an array-append operation
        for (uint32_t i=0; i < inOtherState.nnz_processed; i++){
            r(nnz_processed + i) += inOtherState.r(i);
            c(nnz_processed + i) += inOtherState.c(i);
            v(nnz_processed + i) += inOtherState.v(i);
        }
        nnz_processed += inOtherState.nnz_processed;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        nnz_processed = 0;
        r.fill(0);
        c.fill(0);
        v.fill(0);
        b.fill(0);
        b_stored.fill(0);
    }

private:
    static inline size_t arraySize(const uint32_t innumEquations,
                                   const uint32_t inNNZA) {
        return 7 + 3 * inNNZA + 2 * innumEquations;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param innumVars The number of independent variables.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: numVars                (Total number of variables)
     * - 1: numEquations           (Total number of equations)
     * - 2: nnZ                    (Total number of non-zeros)
     * - 3: algorithm
     * - 4: maxIter
     * - 5: termToler
     * - 6: nnz_processed          Number of non-zeros processed by a node
     * - 7: b                      (RHS vector)
     * - 7: b_stored               (Boolean: Has the b_vector already been loaded?)
     * - 7 + 2 * numEquations: r (LHS matrix row)
     * - 7 + 2 * numEquations + 1 * nnzA: c (LHS matrix column)
     * - 7 + 2 * numEquations + 2 * nnzA: v (LHS matrix value)
     */
    void rebind(uint32_t innumEquations, uint32_t inNNZA) {
        numVars.rebind(&mStorage[0]);
        numEquations.rebind(&mStorage[1]);
        NNZA.rebind(&mStorage[2]);
        algorithm.rebind(&mStorage[3]);
        nnz_processed.rebind(&mStorage[4]);
        maxIter.rebind(&mStorage[5]);
        termToler.rebind(&mStorage[6]);
        b.rebind(&mStorage[7], innumEquations);
        b_stored.rebind(&mStorage[7 + innumEquations], innumEquations);
        r.rebind(&mStorage[7 + 2*innumEquations], inNNZA);
        c.rebind(&mStorage[7 + 2*innumEquations + inNNZA], inNNZA);
        v.rebind(&mStorage[7 + 2*innumEquations + 2 * inNNZA], inNNZA);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt32 numVars;
    typename HandleTraits<Handle>::ReferenceToUInt32 numEquations;
    typename HandleTraits<Handle>::ReferenceToUInt32 NNZA;
    typename HandleTraits<Handle>::ReferenceToUInt32 nnz_processed;
    typename HandleTraits<Handle>::ReferenceToUInt32 algorithm;
    typename HandleTraits<Handle>::ReferenceToUInt32 maxIter;
    typename HandleTraits<Handle>::ReferenceToDouble termToler;

    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap b_stored;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap b;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap r;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap c;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap v;
};


/**
 * @brief Perform the sparse linear system transition step
 */
AnyType
sparse_inmem_iterative_linear_system_transition::run(AnyType &args) {

    SparseInMemIterativeLinearSystemTransitionState<MutableArrayHandle<double> > 
                                                              state = args[0];
    int32_t row_id = args[1].getAs<int32_t>();
    int32_t col_id = args[2].getAs<int32_t>();
    double value = args[3].getAs<double>();
    double _b = args[4].getAs<double>();


    // When the segment recieves the first non-zero in the sparse matrix
    // we initialize the state
    if (state.nnz_processed == 0) {
        
        int32_t num_equations = args[5].getAs<int32_t>();
        int32_t num_vars = args[6].getAs<int32_t>();
        int32_t total_nnz = args[7].getAs<int32_t>();
        int algorithm = args[8].getAs<int>();
        int32_t maxIter = args[9].getAs<int>();
        double termToler = args[10].getAs<double>();

        state.initialize(*this, 
                        num_vars,
                        num_equations,
                        total_nnz
                        );

        state.algorithm = algorithm;
        state.NNZA = total_nnz;
        state.numVars = num_vars;
        state.numEquations = num_equations;
        state.maxIter = maxIter;
        state.termToler = termToler;
        state.b_stored.setZero();
        state.b.setZero();

    }

	// Now do the transition step
	// First we create a block of zeros in memory
	// and then add the vector in the appropriate position
    ColumnVector bvec(static_cast<uint32_t>(state.numEquations));
    ColumnVector rvec(static_cast<uint32_t>(state.NNZA));
    ColumnVector cvec(static_cast<uint32_t>(state.NNZA));
    ColumnVector vvec(static_cast<uint32_t>(state.NNZA));
    
    bvec.setZero();
    rvec.setZero();
    cvec.setZero();
    vvec.setZero();

    rvec(state.nnz_processed) = row_id;
    cvec(state.nnz_processed) = col_id;
    vvec(state.nnz_processed) = value;

    if (state.b_stored(row_id) == 0) {
        bvec(row_id) = _b;
        state.b_stored(row_id) = 1; 
        state.b += bvec;
    }

    // Build the vector & matrices based on row_id
    state.r += rvec;
    state.c += cvec;
    state.v += vvec;
    state.nnz_processed++;

    return state;
}


/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
sparse_inmem_iterative_linear_system_merge_states::run(AnyType &args) {
    SparseInMemIterativeLinearSystemTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    SparseInMemIterativeLinearSystemTransitionState<ArrayHandle<double> > stateRight = args[1];

    // We first handle the trivial case where this function is called with one
    // of the states being the initial state
    if (stateLeft.numEquations == 0)
        return stateRight;
    else if (stateRight.numEquations == 0)
        return stateLeft;

    // Merge states together and return
    stateLeft += stateRight;
    return stateLeft;
}



/**
 * @brief Perform the linear system final step
 */
AnyType
sparse_inmem_iterative_linear_system_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    SparseInMemIterativeLinearSystemTransitionState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.numEquations == 0)
        return Null();
    
    // Eigen works better when you reserve the number of nnz
    // Note: When calling reserve(), it is not required that nnz is the 
    //       exact number of nonzero elements in the final matrix. However, an 
    //       exact estimation will avoid multiple reallocations during the 
    //       insertion phase.
    SparseMatrix A(state.numEquations, state.numVars);
    A.reserve(state.NNZA);
    ColumnVector x;

    for (uint32_t i=0; i < state.NNZA; i++){
        A.insert(static_cast<uint32_t>(state.r(i)),
                 static_cast<uint32_t>(state.c(i))) = state.v(i);
    }


    int iters = 0;
    double error = 0.;


    // Switch case needs scoping in C++ if you want to declare inside it
    // Unfortunately, this means that I have to write the code to call the 
    // solver in each switch case separtely
    // Note: CG uses a diagonal preconditioner by default
    switch (state.algorithm) {
      case 1:{
              Eigen::ConjugateGradient<SparseMatrix> solver;
              solver.setTolerance(state.termToler);
              solver.setMaxIterations(static_cast<int>(state.maxIter));
              x = solver.compute(A).solve(state.b);
              iters = solver.iterations();
              error = solver.error();
              break;
      }
      case 2:{
              Eigen::BiCGSTAB<SparseMatrix> solver;
              solver.setTolerance(state.termToler);
              solver.setMaxIterations(static_cast<int>(state.maxIter));
              x = solver.compute(A).solve(state.b);
              iters = solver.iterations();
              error = solver.error();
              break;
      }
      // Preconditioned CG: Uses an incomplete LUT preconditioner
      // For the incomplete LUT preconditioner. You might want to see Eigen docs
      // for factors like fill-in which will make the algorithm more suitable
      // for handling tougher linear systgems
      case 3:{
              // 3 Arguments in template
              //  ConjugateGradient< _MatrixType, _UpLo, _Preconditioner >:
              Eigen::ConjugateGradient<SparseMatrix, 1,
                                        Eigen::IncompleteLUT<double> > solver;
              solver.setTolerance(state.termToler);
              solver.setMaxIterations(static_cast<int>(state.maxIter));
              x = solver.compute(A).solve(state.b);
              iters = solver.iterations();
              error = solver.error();
              break;
      }
      case 4:{
              // 2 Arguments in template: 
              //  ConjugateGradient< _MatrixType, _UpLo, _Preconditioner >:
              // NOTE: BiCGSTAB is a bi-conjugate gradient that does not 
              // require a lower or upper triangular option
              Eigen::BiCGSTAB<SparseMatrix, Eigen::IncompleteLUT<double> > solver;
              solver.setTolerance(state.termToler);
              solver.setMaxIterations(static_cast<int>(state.maxIter));
              x = solver.compute(A).solve(state.b);
              iters = solver.iterations();
              error = solver.error();
              break;
      }
    }


    // Compute the residual
    return inmem_iterative_sparse_stateToResult(*this, x, iters, error);
}

/**
 * @brief Helper function that computes the final statistics 
 */

AnyType inmem_iterative_sparse_stateToResult(
    const Allocator &inAllocator,
	const ColumnVector &x,
	const int iters,
	const double residual_norm) {

	MutableNativeColumnVector solution(
        inAllocator.allocateArray<double>(x.size()));

    for (Index i = 0; i < x.size(); ++i) {
        solution(i) = x(i);
    }

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << solution 
          << residual_norm
          << iters;

    return tuple;
}





} // namespace linear_systems

} // namespace modules

} // namespace madlib
