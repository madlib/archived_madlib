/* ------------------------------------------------------
 *
 * @file marginal.cpp
 *
 * @brief Marginal effects for regression functions
 *
 *
 *//* ----------------------------------------------------------------------- */
#include <limits>
#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>
#include <boost/math/distributions.hpp>
#include <modules/prob/student.hpp>
#include "marginal.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

namespace regress {

inline double logistic(double x) {
    return 1. / (1. + std::exp(-x));
}

/**
 * @brief Helper function that computes the final statistics for the marginal variance
 */

AnyType margins_stateToResult(
    const Allocator &inAllocator,
    const ColumnVector &diagonal_of_variance_matrix,
    const ColumnVector &inmarginal_effects_per_observation,
    const double numRows) {

    uint16_t n_basis_terms = static_cast<uint16_t>(inmarginal_effects_per_observation.size());
    MutableNativeColumnVector marginal_effects(
        inAllocator.allocateArray<double>(n_basis_terms));
    MutableNativeColumnVector stdErr(
        inAllocator.allocateArray<double>(n_basis_terms));
    MutableNativeColumnVector tStats(
        inAllocator.allocateArray<double>(n_basis_terms));
    MutableNativeColumnVector pValues(
        inAllocator.allocateArray<double>(n_basis_terms));

    for (Index i = 0; i < n_basis_terms; ++i) {
        marginal_effects(i) = inmarginal_effects_per_observation(i) / numRows;
        stdErr(i) = std::sqrt(diagonal_of_variance_matrix(i));
        tStats(i) = marginal_effects(i) / stdErr(i);

        // p-values only make sense if numRows > coef.size()
        if (numRows > n_basis_terms)
            pValues(i) = 2. * prob::cdf( prob::normal(),
                                         -std::abs(tStats(i)));
    }

    // Return all coefficients, standard errors, etc. in a tuple
    // Note: p-values will return NULL if numRows <= coef.size
    AnyType tuple;
    tuple << marginal_effects
          << stdErr
          << tStats
          << (numRows > n_basis_terms? pValues: Null());
    return tuple;
}
// -------------------------------------------------------------------------


// ---------------------------------------------------------------------------
//             Marginal Effects Linear Regression States
// ---------------------------------------------------------------------------

/**
 * @brief State for marginal effects calculation for logistic regression
 *
 * TransitionState encapsualtes the transition state during the
 * marginal effects calculation for the logistic-regression aggregate function.
 * To the database, the state is exposed as a single DOUBLE PRECISION array,
 * to the C++ code it is a proper object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elemenets are 0.
 *
 */
template <class Handle>
class MarginsLinregrInteractionState {
    template <class OtherHandle>
    friend class MarginsLinregrInteractionState;

  public:
    MarginsLinregrInteractionState(const AnyType &inArray)
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
     * @brief Initialize the marginal variance calculation state.
     *
     * This function is only called for the first iteration, for the first row.
     */
    inline void initialize(const Allocator &inAllocator,
                           const uint16_t inWidthOfX,
                           const uint16_t inNumBasis) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                                             dbal::DoZero, dbal::ThrowBadAlloc>(
                arraySize(inWidthOfX, inNumBasis));
        rebind(inWidthOfX, inNumBasis);
        widthOfX = inWidthOfX;
        numBasis = inNumBasis;
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    MarginsLinregrInteractionState &operator=(
        const MarginsLinregrInteractionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    MarginsLinregrInteractionState &operator+=(
        const MarginsLinregrInteractionState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                                   "states");
        numRows += inOtherState.numRows;
        marginal_effects += inOtherState.marginal_effects;
        delta += inOtherState.delta;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        training_data_vcov.fill(0);
        marginal_effects.fill(0);
        delta.fill(0);
    }

  private:
    static inline size_t arraySize(const uint16_t inWidthOfX,
                                   const uint16_t inNumBasis) {
        return 4 + inNumBasis + (inWidthOfX + inNumBasis) * inWidthOfX;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX The number of independent variables.
     *
     */
    void rebind(uint16_t inWidthOfX, uint16_t inNumBasis) {
        iteration.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        numBasis.rebind(&mStorage[2]);
        numRows.rebind(&mStorage[3]);
        marginal_effects.rebind(&mStorage[4], inNumBasis);
        training_data_vcov.rebind(&mStorage[4 + inNumBasis], inWidthOfX, inWidthOfX);
        delta.rebind(&mStorage[4 + inNumBasis + inWidthOfX * inWidthOfX],
                     inNumBasis, inWidthOfX);
    }
    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt32 iteration;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToUInt16 numBasis;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap marginal_effects;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap training_data_vcov;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap delta;
};
// ----------------------------------------------------------------------

/**
 * @brief Perform the marginal effects transition step
 */
AnyType
margins_linregr_int_transition::run(AnyType &args) {
    // Early return because of an exception has been "thrown"
    // (actually "warning") in the previous invocations
    if (args[0].isNull())
        return Null();
    MarginsLinregrInteractionState<MutableArrayHandle<double> > state = args[0];
    if (args[1].isNull() || args[2].isNull() ||
            args[3].isNull() || args[4].isNull()) {
        return args[0];
    }
    MappedColumnVector x;
    try {
        // an exception is raised in the backend if args[2] contains nulls
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    // The following check was added with MADLIB-138.
    if (!dbal::eigen_integration::isfinite(x)) {
        //throw std::domain_error("Design matrix is not finite.");
        warning("Design matrix is not finite.");
        return Null();
     }

    MappedColumnVector beta = args[2].getAs<MappedColumnVector>();

    Matrix J_trans = args[4].getAs<MappedMatrix>();
    J_trans.transposeInPlace(); // we actually pass-in J but transpose since
                                // we only require J^T in our equations

    if (state.numRows == 0) {
        if (x.size() > std::numeric_limits<uint16_t>::max()) {
            //throw std::domain_error("Number of independent variables cannot be "
            //                        "larger than 65535.");
            warning("Number of independent variables cannot be larger than 65535.");
            return Null();
        }
        state.initialize(*this,
                         static_cast<uint16_t>(beta.size()),
                         static_cast<uint16_t>(J_trans.rows()));
        Matrix training_data_vcov = args[3].getAs<MappedMatrix>();
        state.training_data_vcov = training_data_vcov;
    }

    // Now do the transition step
    state.numRows++;

    // compute marginal effects and delta using 1st and 2nd derivatives
    state.marginal_effects += J_trans * beta;
    state.delta += J_trans;
    return state;
}


/**
 * @brief Marginal effects: Merge transition states
 */
AnyType
margins_linregr_int_merge::run(AnyType &args) {
    // In case the aggregator should be terminated because
    // an exception has been "thrown" in the transition function
    if (args[0].isNull() || args[1].isNull())
        return Null();
    MarginsLinregrInteractionState<MutableArrayHandle<double> > stateLeft = args[0];
    MarginsLinregrInteractionState<ArrayHandle<double> > stateRight = args[1];
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
 * @brief Marginal effects: Final step
 */
AnyType
margins_linregr_int_final::run(AnyType &args) {
    // In case the aggregator should be terminated because
    // an exception has been "thrown" in the transition function
    if (args[0].isNull())
        return Null();
    // We request a mutable object.
    // Depending on the backend, this might perform a deep copy.
    MarginsLinregrInteractionState<ArrayHandle<double> > state = args[0];
    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    // Variance of the marginal effects (computed by delta method)
    Matrix variance;
    variance = state.delta * state.training_data_vcov;
    // we only need the diagonal elements of the variance, so we perform a dot
    // product of each row with itself to compute each diagonal element.
    ColumnVector variance_diagonal =
        variance.cwiseProduct(state.delta).rowwise().sum() / static_cast<double>(state.numRows * state.numRows);

    // Computing the marginal effects
    return margins_stateToResult(*this, variance_diagonal,
                                 state.marginal_effects, static_cast<double>(state.numRows));
}

// ---------------------------------------------------------------------------
//             Marginal Effects Logistic Regression States
// ---------------------------------------------------------------------------
/**
 * @brief State for marginal effects calculation for logistic regression
 *
 * TransitionState encapsualtes the transition state during the
 * marginal effects calculation for the logistic-regression aggregate function.
 * To the database, the state is exposed as a single DOUBLE PRECISION array,
 * to the C++ code it is a proper object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elemenets are 0.
 *
 */
template <class Handle>
class MarginsLogregrInteractionState {
    template <class OtherHandle>
    friend class MarginsLogregrInteractionState;

  public:
    MarginsLogregrInteractionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[1]),
               static_cast<uint16_t>(mStorage[2]),
               static_cast<uint16_t>(mStorage[3]));
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
     * @brief Initialize the marginal variance calculation state.
     *
     * This function is only called for the first iteration, for the first row.
     */
    inline void initialize(const Allocator &inAllocator,
                           const uint16_t inWidthOfX,
                           const uint16_t inNumBasis,
                           const uint16_t inNumCategoricals) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                                             dbal::DoZero, dbal::ThrowBadAlloc>(
                arraySize(inWidthOfX, inNumBasis, inNumCategoricals));
        rebind(inWidthOfX, inNumBasis, inNumCategoricals);
        widthOfX = inWidthOfX;
        numBasis = inNumBasis;
        numCategoricalVarsInSubset = inNumCategoricals;
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    MarginsLogregrInteractionState &operator=(
        const MarginsLogregrInteractionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    MarginsLogregrInteractionState &operator+=(
        const MarginsLogregrInteractionState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                                   "states");

        numRows += inOtherState.numRows;
        marginal_effects += inOtherState.marginal_effects;
        delta += inOtherState.delta;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        marginal_effects.fill(0);
        categorical_basis_indices.fill(0);
        training_data_vcov.fill(0);
        delta.fill(0);
    }

  private:
    static inline size_t arraySize(const uint16_t inWidthOfX,
                                   const uint16_t inNumBasis,
                                   const uint16_t inNumCategoricals) {
        return 5 + inNumBasis + inNumCategoricals + (inWidthOfX + inNumBasis) * inWidthOfX;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX The number of independent variables.
     *
     */
    void rebind(uint16_t inWidthOfX, uint16_t inNumBasis, uint16_t inNumCategoricals) {
        iteration.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        numBasis.rebind(&mStorage[2]);
        numCategoricalVarsInSubset.rebind(&mStorage[3]);
        numRows.rebind(&mStorage[4]);
        marginal_effects.rebind(&mStorage[5], inNumBasis);
        training_data_vcov.rebind(&mStorage[5 + inNumBasis], inWidthOfX, inWidthOfX);
        delta.rebind(&mStorage[5 + inNumBasis + inWidthOfX * inWidthOfX],
                     inNumBasis, inWidthOfX);
        if (inNumCategoricals > 0)
            categorical_basis_indices.rebind(&mStorage[5 + inNumBasis +
                                            (inWidthOfX + inNumBasis) * inWidthOfX],
                                            inNumCategoricals);
    }
    Handle mStorage;

  public:

    typename HandleTraits<Handle>::ReferenceToUInt32 iteration;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToUInt16 numBasis;
    typename HandleTraits<Handle>::ReferenceToUInt16 numCategoricalVarsInSubset;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap marginal_effects;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap categorical_basis_indices;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap training_data_vcov;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap delta;
};
// ----------------------------------------------------------------------

/**
 * @brief Perform the marginal effects transition step
 */
AnyType
margins_logregr_int_transition::run(AnyType &args) {
    // Early return because of an exception has been "thrown"
    // (actually "warning") in the previous invocations
    if (args[0].isNull())
        return Null();
    MarginsLogregrInteractionState<MutableArrayHandle<double> > state = args[0];
    if (args[1].isNull() || args[2].isNull() ||
            args[3].isNull() || args[4].isNull()) {
        return args[0];
    }    MappedColumnVector f;
    try {
        // an exception is raised in the backend if args[2] contains nulls
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        f.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    // The following check was added with MADLIB-138.
    if (!dbal::eigen_integration::isfinite(f)) {
        //throw std::domain_error("Design matrix is not finite.");
        warning("Design matrix is not finite.");
        return Null();
    }

    // beta is the coefficient vector from logistic regression
    MappedColumnVector beta = args[2].getAs<MappedColumnVector>();

    // basis_indices represents the indices (from beta) of which we want to
    // compute the marginal effect. We don't need to compute the ME for all
    // variables, thus basis_indices could be a subset of all indices.
    MappedColumnVector basis_indices = args[4].getAs<MappedColumnVector>();

    // below symbols match the ones used in the design doc
    const uint16_t N = static_cast<uint16_t>(beta.size());
    const uint16_t M = static_cast<uint16_t>(basis_indices.size());
    assert(N >= M);

    Matrix J;  // J: N * M
    if (args[5].isNull()){
        J = Matrix::Zero(N, M);
        for (Index i = 0; i < M; ++i)
            J(static_cast<Index>(basis_indices(i)), i) = 1;
    } else{
        J = args[5].getAs<MappedMatrix>();
    }
    assert(J.rows() == N && J.cols() == M);

    MappedColumnVector categorical_indices;
    uint16_t numCategoricalVars = 0;

    if (!args[6].isNull()) {
        // categorical_indices represents which indices (from beta) are
        // categorical variables
        try {
            MappedColumnVector xx = args[6].getAs<MappedColumnVector>();
            categorical_indices.rebind(xx.memoryHandle(), xx.size());
        } catch (const ArrayWithNullException &e) {
             //throw std::runtime_error("The categorical indices contain NULL values");
             warning("The categorical indices contain NULL values");
             return Null();
        }
        numCategoricalVars = static_cast<uint16_t>(categorical_indices.size());
    }

    if (state.numRows == 0) {
        if (f.size() > std::numeric_limits<uint16_t>::max()) {
            //throw std::domain_error("Number of independent variables cannot be "
            //                        "larger than 65535.");
            warning("Number of independent variables cannot be larger than 65535.");
            return Null();
        }
        std::vector<uint16_t> tmp_cat_basis_indices;
        if (numCategoricalVars > 0){
            // find which of the variables in basis_indices are categorical
            // and store only the indices for these variables in our state
            for (Index i = 0; i < basis_indices.size(); ++i){
                for (Index j = 0; j < categorical_indices.size(); ++j){
                    if (basis_indices(i) == categorical_indices(j)){
                        tmp_cat_basis_indices.push_back(static_cast<uint16_t>(i));
                        continue;
                    }
                }
            }
            state.numCategoricalVarsInSubset = static_cast<uint16_t>(tmp_cat_basis_indices.size());
        }
        state.initialize(*this,
                         static_cast<uint16_t>(N),
                         static_cast<uint16_t>(M),
                         static_cast<uint16_t>(state.numCategoricalVarsInSubset));

        Matrix training_data_vcov = args[3].getAs<MappedMatrix>();
        state.training_data_vcov = training_data_vcov;

        if (state.numCategoricalVarsInSubset > 0){
            for (int i=0; i < state.numCategoricalVarsInSubset; ++i){
                state.categorical_basis_indices(i) = tmp_cat_basis_indices[i];
            }
        }
    }

    // Now do the transition step
    state.numRows++;
    double f_beta = dot(f, beta);
    double p = std::exp(f_beta)/ (1 + std::exp(f_beta));

    // compute marginal effects and delta using 1st and 2nd derivatives
    ColumnVector J_trans_beta;
    J_trans_beta = trans(J) * beta;
    ColumnVector curr_margins = J_trans_beta * p * (1 - p);

    Matrix curr_delta;
    curr_delta = p * (1 - p) * (trans(J) +
                                (1 - 2 * p) * J_trans_beta * trans(f));

    // margins and delta using discrete differences for categoricals variables

    // f_set_mat and f_unset_mat are matrices where each row corresponds to a
    //  categorical basis variable and the columns correspond to all terms
    // (basis and interaction)
    Matrix f_set_mat;  // numCategoricalVarsInSubset x N
    Matrix f_unset_mat;  // numCategoricalVarsInSubset x N
    if (!args[7].isNull() && !args[8].isNull()){
        // the matrix is read in column-order but passed in row-order
        f_set_mat = args[7].getAs<MappedMatrix>();
        f_set_mat.transposeInPlace();

        f_unset_mat = args[8].getAs<MappedMatrix>();
        f_unset_mat.transposeInPlace();
    }

    // PERFORMANCE TWEAK: for the no interaction case, f_set_mat and f_unset_mat
    // only need column entries for the categorical variables (others are same
    // as f). Since, passing a smaller matrix into the transition function is
    // faster, for the no interaction case, we don't need to input the whole
    // matrix into this function. For the interaction case, since it is unknown
    // which indices have interaction, we require entries for all columns in the
    // matrices.
    bool no_interactions = (f_set_mat.cols() < N);
    for (Index i = 0; i < state.numCategoricalVarsInSubset; ++i) {
        // Note: categorical_indices are assumed to be zero-based
        ColumnVector f_set;
        ColumnVector f_unset;
        ColumnVector shortened_f_set = f_set_mat.row(i);
        ColumnVector shortened_f_unset = f_unset_mat.row(i);

        if (no_interactions){
            f_set = f;
            f_unset = f;
            for (Index j=0; j < shortened_f_set.size(); ++j){
                f_set(static_cast<Index>(categorical_indices(j))) = shortened_f_set(j);
                f_unset(static_cast<Index>(categorical_indices(j))) = shortened_f_unset(j);
            }
        } else {
            f_set = shortened_f_set;
            f_unset = shortened_f_unset;
        }
        double p_set = logistic(dot(f_set, beta));
        double p_unset = logistic(dot(f_unset, beta));

        curr_margins(static_cast<uint16_t>(state.categorical_basis_indices(i))) = p_set - p_unset;
        curr_delta.row(static_cast<uint16_t>(state.categorical_basis_indices(i))) = (
            p_set * (1 - p_set) * f_set - p_unset * (1 - p_unset) * f_unset);
    }
    state.marginal_effects += curr_margins;
    state.delta += curr_delta;
    return state;
}


/**
 * @brief Marginal effects: Merge transition states
 */
AnyType
margins_logregr_int_merge::run(AnyType &args) {
    // In case the aggregator should be terminated because
    // an exception has been "thrown" in the transition function
    if (args[0].isNull() || args[1].isNull())
        return Null();
    MarginsLogregrInteractionState<MutableArrayHandle<double> > stateLeft = args[0];
    MarginsLogregrInteractionState<ArrayHandle<double> > stateRight = args[1];
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
 * @brief Marginal effects: Final step
 */
AnyType
margins_logregr_int_final::run(AnyType &args) {
    // In case the aggregator should be terminated because
    // an exception has been "thrown" in the transition function
    if (args[0].isNull())
        return Null();
    // We request a mutable object.
    // Depending on the backend, this might perform a deep copy.
    MarginsLogregrInteractionState<MutableArrayHandle<double> > state = args[0];
    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    // Variance for marginal effects according to the delta method
    Matrix variance;
    variance = state.delta * state.training_data_vcov;

    // we only need the diagonal elements of the variance, so we perform a dot
    // product of each row with state.delta to compute each diagonal element.
    // We divide by numRows^2 since we need the average variance
    ColumnVector variance_diagonal =
        variance.cwiseProduct(state.delta).rowwise().sum() / static_cast<double>(state.numRows * state.numRows);

    // Computing the final results
    return margins_stateToResult(*this, variance_diagonal,
                                 state.marginal_effects, static_cast<double>(state.numRows));
}
// ------------------------ End of Logistic Marginal ---------------------------

// ---------------------------------------------------------------------------
//             Marginal Effects Multilogistic Regression
// ---------------------------------------------------------------------------
/**
 * @brief State for marginal effects calculation for multilogistic regression
 *
 * TransitionState encapsualtes the transition state during the
 * marginal effects calculation for the logistic-regression aggregate function.
 * To the database, the state is exposed as a single DOUBLE PRECISION array,
 * to the C++ code it is a proper object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elemenets are 0.
 *
 */
template <class Handle>
class MarginsMLogregrInteractionState {
    template <class OtherHandle>
    friend class MarginsMLogregrInteractionState;

  public:
    MarginsMLogregrInteractionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[0]),
               static_cast<uint16_t>(mStorage[1]),
               static_cast<uint16_t>(mStorage[2]),
               static_cast<uint16_t>(mStorage[3]));
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
     * @brief Initialize the marginal variance calculation state.
     *
     * This function is only called for the first iteration, for the first row.
     */
    inline void initialize(const Allocator &inAllocator,
                           const uint16_t inWidthOfX,
                           const uint16_t inNumCategories,
                           const uint16_t inNumBasis,
                           const uint16_t inNumCategoricalVars) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                                             dbal::DoZero, dbal::ThrowBadAlloc>(
                arraySize(inWidthOfX, inNumCategories, inNumBasis, inNumCategoricalVars));
        rebind(inWidthOfX,  inNumCategories, inNumBasis, inNumCategoricalVars);
        widthOfX = inWidthOfX;
        numCategories = inNumCategories;
        numBasis = inNumBasis;
        numCategoricalVarsInSubset = inNumCategoricalVars;
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    MarginsMLogregrInteractionState &operator=(
        const MarginsMLogregrInteractionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    MarginsMLogregrInteractionState &operator+=(
        const MarginsMLogregrInteractionState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                                   "states");

        numRows += inOtherState.numRows;
        marginal_effects += inOtherState.marginal_effects;
        delta += inOtherState.delta;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        marginal_effects.fill(0);
        categorical_basis_indices.fill(0);
        training_data_vcov.fill(0);
        delta.fill(0);
    }

  private:
    static inline size_t arraySize(const uint16_t inWidthOfX,
                                   const uint16_t inNumCategories,
                                   const uint16_t inNumBasis,
                                   const uint16_t inNumCategoricalVars) {
        return 5 + (inNumCategories - 1) * (inNumBasis +
                    inNumBasis*inWidthOfX*(inNumCategories - 1) +
                    inWidthOfX*inWidthOfX*(inNumCategories - 1)) +
                inNumCategoricalVars;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX The number of independent variables.
     *
     */
    void rebind(const uint16_t inWidthOfX,
                const uint16_t inNumCategories,
                const uint16_t inNumBasis,
                const uint16_t inNumCategoricalVars) {

        const uint16_t & L = inNumCategories;
        const uint16_t & N = inWidthOfX;
        const uint16_t & M = inNumBasis;

        widthOfX.rebind(&mStorage[0]);
        numCategories.rebind(&mStorage[1]);
        numBasis.rebind(&mStorage[2]);
        numCategoricalVarsInSubset.rebind(&mStorage[3]);
        numRows.rebind(&mStorage[4]);

        if (L == 0) { return; }

        marginal_effects.rebind(&mStorage[5], M, L-1);

        int current_length = 5 + M * (L - 1);

        training_data_vcov.rebind(&mStorage[current_length], N*(L-1), N*(L-1));
        current_length += N * (L - 1) * N * (L - 1);

        delta.rebind(&mStorage[current_length], M*(L-1), N*(L-1));
        current_length += N * (L - 1) * M * (L - 1);

        if (inNumCategoricalVars > 0)
            categorical_basis_indices.rebind(&mStorage[static_cast<uint16_t>(current_length)], inNumCategoricalVars);
    }
    Handle mStorage;

  public:
    // symbols in comments correspond to the design document
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;       // N
    typename HandleTraits<Handle>::ReferenceToUInt16 numCategories;  // L
    typename HandleTraits<Handle>::ReferenceToUInt16 numBasis;       // M
    typename HandleTraits<Handle>::ReferenceToUInt16 numCategoricalVarsInSubset;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap marginal_effects; // ME
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap categorical_basis_indices;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap training_data_vcov;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap delta;   // S
};
// ----------------------------------------------------------------------

namespace {
inline Index
reindex(Index outer, Index inner, Index block) { return outer * block + inner; }
}

/**
 * @brief Perform the marginal effects transition step
 */
AnyType
margins_mlogregr_int_transition::run(AnyType &args) {
    // Early return because of an exception has been "thrown"
    // (actually "warning") in the previous invocations
    if (args[0].isNull())
        return Null();
    MarginsMLogregrInteractionState<MutableArrayHandle<double> > state = args[0];
    if (args[1].isNull() || args[2].isNull() || args[3].isNull() ||
        args[4].isNull()) {
        return args[0];
    }

    MappedColumnVector f;
    try {
        // an exception is raised in the backend if args[1] contains nulls
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        f.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    // The following check was added with MADLIB-138.
    if (!dbal::eigen_integration::isfinite(f)) {
        //throw std::domain_error("Design matrix is not finite.");
        warning("Design matrix is not finite.");
        return Null();
    }

    // coefficients are arranged in a matrix
    MappedMatrix beta = args[2].getAs<MappedMatrix>();  // beta: N x (L - 1)

    // basis_indices represents the indices (from beta) of which we want to
    // compute the marginal effect. We don't need to compute the ME for all
    // variables, thus basis_indices could be a subset of all indices.
    MappedColumnVector basis_indices = args[4].getAs<MappedColumnVector>();

    // all variable symbols correspond to the design document
    const uint16_t N = static_cast<uint16_t>(beta.rows());
    const uint16_t M = static_cast<uint16_t>(basis_indices.size());
    assert(N >= M);

    Matrix J;  // J: N x M
    if (args[5].isNull()){
        J = Matrix::Zero(N, M);
        for (Index i = 0; i < M; ++i)
            J(static_cast<Index>(basis_indices(i)), i) = 1;
    } else{
        J = args[5].getAs<MappedMatrix>();
    }
    assert(J.rows() == N && J.cols() == M);

    MappedColumnVector categorical_indices;
    uint16_t numCategoricalVars = 0;

    if (!args[6].isNull()) {
        // categorical_indices represents which indices (from beta) are
        // categorical variables
        try {
            MappedColumnVector xx = args[6].getAs<MappedColumnVector>();
            categorical_indices.rebind(xx.memoryHandle(), xx.size());
        } catch (const ArrayWithNullException &e) {
             //throw std::runtime_error("The categorical indices contain NULL values");
             warning("The categorical indices contain NULL values");
             return Null();
        }
        numCategoricalVars = static_cast<uint16_t>(categorical_indices.size());
    }

    if (state.numRows == 0) {
        if (f.size() > std::numeric_limits<uint16_t>::max()) {
            //throw std::domain_error("Number of independent variables cannot be "
            //                        "larger than 65535.");
            warning("Number of independent variables cannot be larger than 65535.");
            return Null();
        }
        std::vector<uint16_t> tmp_cat_basis_indices;
        if (numCategoricalVars > 0){
            // find which of the variables in basis_indices are categorical
            // and store only the indices for these variables in our state
            for (Index i = 0; i < basis_indices.size(); ++i){
                for (Index j = 0; j < categorical_indices.size(); ++j){
                    if (basis_indices(i) == categorical_indices(j)){
                        tmp_cat_basis_indices.push_back(static_cast<uint16_t>(i));
                        continue;
                    }
                }
            }
            state.numCategoricalVarsInSubset = static_cast<uint16_t>(tmp_cat_basis_indices.size());
        }
        state.initialize(*this,
                         static_cast<uint16_t>(J.rows()),
                         static_cast<uint16_t>(beta.cols() + 1),
                         static_cast<uint16_t>(J.cols()),
                         static_cast<uint16_t>(state.numCategoricalVarsInSubset));

        Matrix training_data_vcov = args[3].getAs<MappedMatrix>();
        state.training_data_vcov = training_data_vcov;
        if (state.numCategoricalVarsInSubset > 0){
            for (int i=0; i < state.numCategoricalVarsInSubset; ++i){
                state.categorical_basis_indices(i) = tmp_cat_basis_indices[i];
            }
        }
    }

    state.numRows++;

    // all variable symbols correspond to the design document
    const uint16_t & L = state.numCategories;
    ColumnVector prob(trans(beta) * f);
    Matrix J_trans_beta(trans(J) * beta);

    // Calculate the odds ratio
    prob = prob.array().exp();
    double prob_sum = prob.sum();
    prob = prob / (1 + prob_sum);

    ColumnVector JBP(J_trans_beta * prob);
    Matrix curr_margins = J_trans_beta * prob.asDiagonal() - JBP * trans(prob);

    // compute delta using 2nd derivatives
    // delta matrix is 2-D of size (L-1)M x (L-1)N:
    //      row_index = [0, (L-1)M), col_index = [0, (L-1)N)
    // row_index(m, l) = m * (L-1) + l
    // col_index(n, l1) = n * (L-1) + l1
    Index row_index, col_index;
    int delta_l_l1;
    for (int m = 0; m < M; m++){
        // Skip the categorical variables
        if (state.numCategoricalVarsInSubset > 0) {
            bool is_categorical = false;
            for(int i = 0; i < state.categorical_basis_indices.size(); i++)
                if (m == state.categorical_basis_indices(i)) {
                    is_categorical = true;
                    break;
                }
             if (is_categorical)
                continue;
        }

        for (int l=0; l < (L-1); l++){
            row_index = reindex(m, l, L-1);
            for (int n=0; n < N; n++){
                for (int l1=0; l1 < (L-1); l1++){
                    delta_l_l1 = (l==l1) ? 1 : 0;
                    col_index = reindex(n, l1, L-1);
                    state.delta(row_index, col_index) +=
                        f(n)*(delta_l_l1 - prob(l1)) * curr_margins(m, l) +
                        prob(l) * (delta_l_l1 * J(n, m) -
                                   f(n) * curr_margins(m, l1) -
                                   prob(l1) * J(n, m));
                }
            }
        }
    }

    // update marginal effects and delta using discrete differences just for
    // categorical variables
    Matrix f_set_mat;   // numCategoricalVarsInSubset * N
    Matrix f_unset_mat; // numCategoricalVarsInSubset * N
    // the above matrices contain the f_set and f_unset for all categorical variables
    if (!args[7].isNull() && !args[8].isNull()){
        // the matrix is read in column-order but passed in row-order
        f_set_mat = args[7].getAs<MappedMatrix>();
        f_set_mat.transposeInPlace();

        f_unset_mat = args[8].getAs<MappedMatrix>();
        f_unset_mat.transposeInPlace();
    }

    // PERFORMANCE TWEAK: for the no interaction case, f_set_mat and f_unset_mat
    // only need column entries for the categorical variables (others are same
    // as f). Since, passing a smaller matrix into the transition function is
    // faster, for the no interaction case, we don't need to input the whole
    // matrix into this function. For the interaction case, since it is unknown
    // which indices have interaction, we require entries for all columns in the
    // matrices.
    bool no_interactions = (f_set_mat.cols() < N);
    for (Index i = 0; i < state.numCategoricalVarsInSubset; ++i) {
        // Note: categorical_indices are assumed to be zero-based
        ColumnVector f_set;
        ColumnVector f_unset;
        ColumnVector shortened_f_set = f_set_mat.row(i);
        ColumnVector shortened_f_unset = f_unset_mat.row(i);

        if (no_interactions){
            f_set = f;
            f_unset = f;
            for (Index j=0; j < shortened_f_set.size(); ++j){
                f_set(static_cast<Index>(categorical_indices(j))) = shortened_f_set(j);
                f_unset(static_cast<Index>(categorical_indices(j))) = shortened_f_unset(j);
            }
        } else {
            f_set = shortened_f_set;
            f_unset = shortened_f_unset;
        }

        RowVector p_set(trans(f_set) * beta);
        {
            p_set = p_set.array().exp();
            double p_sum = p_set.sum();
            p_set = p_set / (1 + p_sum);
        }

        RowVector p_unset(trans(f_unset) * beta);
        {
            p_unset = p_unset.array().exp();
            double p_sum = p_unset.sum();
            p_unset = p_unset / (1 + p_sum);
        }
        // Compute the marginal effect using difference method
        curr_margins.row(static_cast<uint16_t>(state.categorical_basis_indices(i))) = p_set - p_unset;

        // Compute the delta using difference method
        int m = static_cast<uint16_t>(state.categorical_basis_indices(i));
        for (int l = 0; l < L - 1; l++) {
            row_index = reindex(m, l, L - 1);
            for (int n = 0; n < N; n++) {
                for (int l1 = 0; l1 < L - 1; l1++) {
                    double delta = - p_set(l) * p_set(l1) * f_set(n) + p_unset(l) * p_unset(l1) * f_unset(n);
                    if (l1 == l)
                        delta += p_set(l) * f_set(n) - p_unset(l) * f_unset(n);
                    col_index = reindex(n, l1, L - 1);
                    state.delta(row_index, col_index) += delta;
                }
            }
        }

    }
    state.marginal_effects += curr_margins;
    return state;
}


/**
 * @brief Marginal effects: Merge transition states
 */
AnyType
margins_mlogregr_int_merge::run(AnyType &args) {
    // In case the aggregator should be terminated because
    // an exception has been "thrown" in the transition function
    if (args[0].isNull() || args[1].isNull())
        return Null();
    MarginsMLogregrInteractionState<MutableArrayHandle<double> > stateLeft = args[0];
    MarginsMLogregrInteractionState<ArrayHandle<double> > stateRight = args[1];
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
 * @brief Marginal effects: Final step
 */
AnyType
margins_mlogregr_int_final::run(AnyType &args) {
    // In case the aggregator should be terminated because
    // an exception has been "thrown" in the transition function
    if (args[0].isNull())
        return Null();
    // We request a mutable object.
    // Depending on the backend, this might perform a deep copy.
    MarginsMLogregrInteractionState<MutableArrayHandle<double> > state = args[0];
    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    state.marginal_effects /= static_cast<double>(state.numRows);
    Matrix marginal_effects_trans = trans(state.marginal_effects);
    AnyType tuple;
    tuple << marginal_effects_trans;

    // Variance for marginal effects according to the delta method
    Matrix variance(state.delta * state.training_data_vcov);
    // // we only need the diagonal elements of the variance, so we perform a dot
    // // product of each row with itself to compute each diagonal element.
    // // We divide by numRows^2 since we need the average variance
    Matrix std_err = variance.cwiseProduct(state.delta).rowwise().sum() / static_cast<double>(state.numRows * state.numRows);
    std_err = std_err.array().sqrt();
    std_err.resize(state.numCategories-1, state.numBasis);
    tuple << std_err;

    Matrix t_stats = marginal_effects_trans.cwiseQuotient(std_err);
    tuple << t_stats;

    // Note: p-values will return NULL if numRows <= coef.size
    if (state.numRows > state.numBasis) {
        MutableNativeMatrix p_values(
                this->allocateArray<double>(state.numBasis * (state.numCategories-1)),
                state.numCategories-1,
                state.numBasis);
        for (Index l = 0; l < p_values.rows(); l ++) {
            for (Index m = 0; m < p_values.cols(); m ++) {
                p_values(l,m) = 2. * prob::cdf(prob::normal(), -std::abs(t_stats(l,m)));
            }
        }
        tuple << static_cast<Matrix>(p_values);
    }

    return tuple;
}
// ------------------------ End of Logistic Marginal ---------------------------


} // namespace regress

} // namespace modules

} // namespace madlib
