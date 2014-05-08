/* ------------------------------------------------------
 *
 * @file logistic.cpp
 *
 * @brief Logistic-Regression functions
 *
 * We implement the conjugate-gradient method and the iteratively-reweighted-
 * least-squares method.
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

// FIXME this enum should be accessed by all modules that may need grouping
// valid status values
enum { IN_PROCESS, COMPLETED, TERMINATED, NULL_EMPTY };

inline double logistic(double x) {
    return 1. / (1. + std::exp(-x));
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
        numCategoricals = inNumCategoricals;
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
        categorical_indices.fill(0);
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
        numCategoricals.rebind(&mStorage[3]);
        numRows.rebind(&mStorage[4]);
        marginal_effects.rebind(&mStorage[5], inNumBasis);
        training_data_vcov.rebind(&mStorage[5 + inNumBasis], inWidthOfX, inWidthOfX);
        delta.rebind(&mStorage[5 + inNumBasis + inWidthOfX * inWidthOfX],
                     inNumBasis, inWidthOfX);
        if (inNumCategoricals > 0)
            categorical_indices.rebind(&mStorage[5 + inNumBasis +
                                            (inWidthOfX + inNumBasis) * inWidthOfX],
                                   inNumCategoricals);
    }
    Handle mStorage;

  public:

    typename HandleTraits<Handle>::ReferenceToUInt32 iteration;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToUInt16 numBasis;
    typename HandleTraits<Handle>::ReferenceToUInt16 numCategoricals;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap marginal_effects;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap categorical_indices;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap training_data_vcov;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap delta;
};
// ----------------------------------------------------------------------

/**
 * @brief Helper function that computes the final statistics for the marginal variance
 */

AnyType margins_stateToResult(
    const Allocator &inAllocator,
    const ColumnVector &diagonal_of_variance_matrix,
    const ColumnVector &inmarginal_effects_per_observation,
    const double numRows) {

    uint16_t n_basis_terms = inmarginal_effects_per_observation.size();
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

        // P-values only make sense if numRows > coef.size()
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


/**
 * @brief Perform the marginal effects transition step
 */
AnyType
margins_logregr_int_transition::run(AnyType &args) {
    MarginsLogregrInteractionState<MutableArrayHandle<double> > state = args[0];
    if (args[1].isNull() || args[2].isNull() ||
            args[3].isNull() || args[4].isNull()) {
        return args[0];
    }    MappedColumnVector x;
    try {
        // an exception is raised in the backend if args[2] contains nulls
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    // The following check was added with MADLIB-138.
    if (!dbal::eigen_integration::isfinite(x))
        throw std::domain_error("Design matrix is not finite.");

    MappedColumnVector coef = args[2].getAs<MappedColumnVector>();

    // matrix is read in as a column-order matrix, the input is passed-in as row-order.
    Matrix derivative_matrix = args[4].getAs<MappedMatrix>();
    derivative_matrix.transposeInPlace();

    if (state.numRows == 0) {
        if (x.size() > std::numeric_limits<uint16_t>::max())
            throw std::domain_error("Number of independent variables cannot be "
                                    "larger than 65535.");
        Matrix training_data_vcov = args[3].getAs<MappedMatrix>();

        MappedColumnVector categorical_indices;
        uint16_t numCategoricals = 0;
        if (!args[5].isNull()) {
            try {
                MappedColumnVector xx = args[5].getAs<MappedColumnVector>();
                categorical_indices.rebind(xx.memoryHandle(), xx.size());
            } catch (const ArrayWithNullException &e) {
                throw std::runtime_error("The categorical indices contain NULL values");
            }
            numCategoricals = categorical_indices.size();
        }
        state.initialize(*this,
                         static_cast<uint16_t>(coef.size()),
                         static_cast<uint16_t>(derivative_matrix.rows()),
                         static_cast<uint16_t>(numCategoricals));
        state.training_data_vcov = training_data_vcov;
        if (numCategoricals > 0)
            state.categorical_indices = categorical_indices;
    }

    // Now do the transition step
    state.numRows++;
    double xc = dot(x, coef);
    double p = std::exp(xc)/ (1 + std::exp(xc));

    // compute marginal effects and delta using 1st and 2nd derivatives
    ColumnVector coef_interaction_sum;
    coef_interaction_sum = derivative_matrix * coef;
    ColumnVector current_me = coef_interaction_sum * p * (1 - p);

    Matrix current_delta;
    current_delta = p * (1 - p) * (
                        (1 - 2 * p) * coef_interaction_sum * trans(x) +
                        derivative_matrix);

    // update marginal effects and delta using discrete differences just for
    // categorical variables
    Matrix x_set;
    Matrix x_unset;
    if (!args[6].isNull() && !args[7].isNull()){
        // the matrix is read in column-order but passed in row-order
        x_set = args[6].getAs<MappedMatrix>();
        x_set.transposeInPlace();

        x_unset = args[7].getAs<MappedMatrix>();
        x_unset.transposeInPlace();
    }
    for (Index i = 0; i < state.numCategoricals; ++i) {
        // Note: categorical_indices are assumed to be zero-based
        double xc_set = dot(x_set.row(i), coef);
        double p_set = logistic(xc_set);
        double xc_unset = dot(x_unset.row(i), coef);
        double p_unset = logistic(xc_unset);
        current_me(static_cast<uint16_t>(state.categorical_indices(i))) = p_set - p_unset;

        current_delta.row(static_cast<uint16_t>(state.categorical_indices(i))) = (
               (p_set * (1 - p_set) * x_set.row(i) -
                p_unset * (1 - p_unset) * x_unset.row(i)));
    }

    state.marginal_effects += current_me;
    state.delta += current_delta;
    return state;
}


/**
 * @brief Marginal effects: Merge transition states
 */
AnyType
margins_logregr_int_merge::run(AnyType &args) {
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
    // product of each row with itself to compute each diagonal element.
    // We divide by numRows^2 since we need the average variance
    ColumnVector variance_diagonal =
        variance.cwiseProduct(state.delta).rowwise().sum() / (state.numRows * state.numRows);

    // Computing the final results
    return margins_stateToResult(*this, variance_diagonal,
                                 state.marginal_effects, state.numRows);
}
// ------------------------ End of Logistic Marginal ---------------------------

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
    if (!dbal::eigen_integration::isfinite(x))
        throw std::domain_error("Design matrix is not finite.");

    MappedColumnVector coef = args[2].getAs<MappedColumnVector>();

    // matrix is read in as a column-order matrix, the input is row-order.
    Matrix derivative_matrix = args[4].getAs<MappedMatrix>();
    derivative_matrix.transposeInPlace();

    if (state.numRows == 0) {
        if (x.size() > std::numeric_limits<uint16_t>::max())
            throw std::domain_error("Number of independent variables cannot be "
                                    "larger than 65535.");
        state.initialize(*this,
                         static_cast<uint16_t>(coef.size()),
                         static_cast<uint16_t>(derivative_matrix.rows()));
        Matrix training_data_vcov = args[3].getAs<MappedMatrix>();
        state.training_data_vcov = training_data_vcov;
    }

    // Now do the transition step
    state.numRows++;
    // compute marginal effects and delta using 1st and 2nd derivatives
    state.marginal_effects += derivative_matrix * coef;
    state.delta += derivative_matrix;
    return state;
}


/**
 * @brief Marginal effects: Merge transition states
 */
AnyType
margins_linregr_int_merge::run(AnyType &args) {
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
        variance.cwiseProduct(state.delta).rowwise().sum() / (state.numRows * state.numRows);

    // Computing the marginal effects
    return margins_stateToResult(*this, variance_diagonal,
                                 state.marginal_effects, state.numRows);
}


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
        numCategoricalVars = inNumCategoricalVars;
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
        categorical_indices.fill(0);
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
        numCategoricalVars.rebind(&mStorage[3]);
        numRows.rebind(&mStorage[4]);

        if (L == 0) { return; }

        marginal_effects.rebind(&mStorage[5], M, L-1);

        uint16_t current_length = 5 + M*(L-1);

        training_data_vcov.rebind(&mStorage[current_length], N*(L-1), N*(L-1));
        current_length += N*(L-1)*N*(L-1);

        delta.rebind(&mStorage[current_length], M*(L-1), N*(L-1));
        current_length += N*(L-1)*M*(L-1);

        if (inNumCategoricalVars > 0)
            categorical_indices.rebind(&mStorage[current_length], inNumCategoricalVars);
    }
    Handle mStorage;

  public:
    // symbols in comments correspond to the design document
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;       // N
    typename HandleTraits<Handle>::ReferenceToUInt16 numCategories;  // L
    typename HandleTraits<Handle>::ReferenceToUInt16 numBasis;       // M
    typename HandleTraits<Handle>::ReferenceToUInt16 numCategoricalVars;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap marginal_effects; // ME
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap categorical_indices;
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
    if (!dbal::eigen_integration::isfinite(f))
        throw std::domain_error("Design matrix is not finite.");

    // coefficients are arranged in a matrix
    MappedMatrix beta = args[2].getAs<MappedMatrix>();  // beta: N * (L - 1)
    MappedMatrix J = args[4].getAs<MappedMatrix>(); // J: N * M
    // J.transposeInPlace();

    if (state.numRows == 0) {
        if (f.size() > std::numeric_limits<uint16_t>::max())
            throw std::domain_error("Number of independent variables cannot be "
                                    "larger than 65535.");
        Matrix training_data_vcov = args[3].getAs<MappedMatrix>();

        MappedColumnVector categorical_indices;
        uint16_t numCategoricalVars = 0;
        if (!args[5].isNull()) {
            try {
                MappedColumnVector xx = args[5].getAs<MappedColumnVector>();
                categorical_indices.rebind(xx.memoryHandle(), xx.size());
            } catch (const ArrayWithNullException &e) {
                throw std::runtime_error("The categorical indices contain NULL values");
            }
            numCategoricalVars = categorical_indices.size();
        }
        state.initialize(*this,
                         static_cast<uint16_t>(J.rows()),
                         static_cast<uint16_t>(beta.cols() + 1),
                         static_cast<uint16_t>(J.cols()),
                         static_cast<uint16_t>(numCategoricalVars));

        state.training_data_vcov = training_data_vcov;
        if (numCategoricalVars > 0)
            state.categorical_indices = categorical_indices;
    }

    state.numRows++;

    // all variable symbols correspond to the design document
    const uint16_t & L = state.numCategories;
    const uint16_t & N = state.widthOfX;
    const uint16_t & M = state.numBasis;


    ColumnVector prob = trans(beta) * f;

    Matrix J_trans_beta = trans(J) * beta;

    // Calculate the odds ratio
    prob = prob.array().exp();
    double prob_sum = prob.sum();
    prob = prob / (1 + prob_sum);

    ColumnVector JBP = J_trans_beta * prob;


    Matrix curr_margins = J_trans_beta * prob.asDiagonal() - JBP * trans(prob);


    // compute delta using 2nd derivatives
    // delta matrix is 2-D of size (L-1)M x (L-1)N:
    //      row_index = [0, (L-1)M), col_index = [0, (L-1)N)
    // row_index(m, l) = m * (L-1) + l
    // col_index(n, l1) = n * (L-1) + l1
    int row_index, col_index, delta_l_l1;
    for (int m = 0; m < M; m++){
        // Skip the categorical variables
        if (state.numCategoricalVars > 0) {
            bool is_categorical = false;
            for(int i = 0; i < state.categorical_indices.size(); i++)
                if (m == state.categorical_indices(i)) {
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
    Matrix f_set_mat;   // numCategoricalVars * N
    Matrix f_unset_mat; // numCategoricalVars * N
    // the above matrices contain the f_set and f_unset for all categorical variables
    if (!args[6].isNull() && !args[7].isNull()){
        // the matrix is read in column-order but passed in row-order
        f_set_mat = args[6].getAs<MappedMatrix>();
        f_set_mat.transposeInPlace();

        f_unset_mat = args[7].getAs<MappedMatrix>();
        f_unset_mat.transposeInPlace();
    }

    for (Index i = 0; i < state.numCategoricalVars; ++i) {
        // Note: categorical_indices are assumed to be zero-based
        RowVector p_set = f_set_mat.row(i) * beta;
        {
            p_set = p_set.array().exp();
            double p_sum = p_set.sum();
            p_set = p_set / (1 + p_sum);
        }

        RowVector p_unset = f_unset_mat.row(i) * beta;
        {
            p_unset = p_unset.array().exp();
            double p_sum = p_unset.sum();
            p_unset = p_unset / (1 + p_sum);
        }
        // Compute the marginal effect using difference method
        curr_margins.row(static_cast<uint16_t>(state.categorical_indices(i))) = p_set - p_unset;

        // Compute the delta using difference method
        int m = static_cast<uint16_t>(state.categorical_indices(i));
        for (int l = 0; l < L - 1; l++) {
            row_index = reindex(m, l, L - 1);
            for (int n = 0; n < N; n++) {
                for (int l1 = 0; l1 < L - 1; l1++) {
                    double delta = - p_set(l) * p_set(l1) * f_set_mat(i, n) + p_unset(l) * p_unset(l1) * f_unset_mat(i, n);
                    if (l1 == l)
                        delta += p_set(l) * f_set_mat(i, n) - p_unset(l) * f_unset_mat(i, n);
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
    // We request a mutable object.
    // Depending on the backend, this might perform a deep copy.
    MarginsMLogregrInteractionState<MutableArrayHandle<double> > state = args[0];
    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    state.marginal_effects /= state.numRows;
    Matrix marginal_effects_trans = trans(state.marginal_effects);
    AnyType tuple;
    tuple << marginal_effects_trans;

    // Variance for marginal effects according to the delta method
    Matrix variance = state.delta * state.training_data_vcov;
    // // we only need the diagonal elements of the variance, so we perform a dot
    // // product of each row with itself to compute each diagonal element.
    // // We divide by numRows^2 since we need the average variance
    Matrix std_err = variance.cwiseProduct(state.delta).rowwise().sum() / (state.numRows * state.numRows);
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
