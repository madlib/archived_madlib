/* ----------------------------------------------------------------------- */
/**
 *
 * @file cox_proportional_hazards.cpp
 *
 * @brief Cox proportional hazards
 *
 * ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>
#include <math.h>

#include "marginal_cox.hpp"

namespace madlib {
namespace modules {
namespace stats {

// Use Eigen
using namespace dbal::eigen_integration;
// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

using namespace std;

// ---------------------------------------------------------------------------
//            Marginal Effects Cox Proportional Hazard State
// ---------------------------------------------------------------------------
/**
 * @brief State for marginal effects calculation for cox proportional hazards
 *
 * TransitionState encapsualtes the transition state during the
 * marginal effects calculation.
 * To the database, the state is exposed as a single DOUBLE PRECISION array,
 * to the C++ code it is a proper object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elemenets are 0.
 *
 */
template <class Handle>
class MarginsCoxPropHazardState {
    template <class OtherHandle>
    friend class MarginsCoxPropHazardState;

  public:
    MarginsCoxPropHazardState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {
        rebind(static_cast<uint16_t>(mStorage[0]),
               static_cast<uint16_t>(mStorage[1]),
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
    MarginsCoxPropHazardState &operator=(
        const MarginsCoxPropHazardState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    MarginsCoxPropHazardState &operator+=(
        const MarginsCoxPropHazardState<OtherHandle> &inOtherState) {

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
        baseline_hazard = 0;
        marginal_effects.fill(0);
        training_data_vcov.fill(0);
        delta.fill(0);
        if (numCategoricalVarsInSubset > 0)
            categorical_basis_indices.fill(0);
    }

  private:
    static inline size_t arraySize(const uint16_t inWidthOfX,
                                   const uint16_t inNumBasis,
                                   const uint16_t inNumCategoricalVars) {
        return 5 + inNumBasis + inNumCategoricalVars +
                (inWidthOfX + inNumBasis) * inWidthOfX;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX The number of independent variables.
     *
     */
    void rebind(const uint16_t inWidthOfX,
                const uint16_t inNumBasis,
                const uint16_t inNumCategoricalVars) {
        widthOfX.rebind(&mStorage[0]);
        numBasis.rebind(&mStorage[1]);
        numCategoricalVarsInSubset.rebind(&mStorage[2]);
        numRows.rebind(&mStorage[3]);
        baseline_hazard.rebind(&mStorage[4]);
        marginal_effects.rebind(&mStorage[5], inNumBasis);
        training_data_vcov.rebind(&mStorage[5 + inNumBasis], inWidthOfX, inWidthOfX);
        delta.rebind(&mStorage[5 + inNumBasis + inWidthOfX*inWidthOfX],
                     inNumBasis, inWidthOfX);
        if (inNumCategoricalVars > 0)
            categorical_basis_indices.rebind(&mStorage[5 + inNumBasis +
                                            (inWidthOfX + inNumBasis)*inWidthOfX],
                                            inNumCategoricalVars);
    }
    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToUInt16 numBasis;
    typename HandleTraits<Handle>::ReferenceToUInt16 numCategoricalVarsInSubset;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToDouble baseline_hazard;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap marginal_effects;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap training_data_vcov;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap delta;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap categorical_basis_indices;
};
// ----------------------------------------------------------------------

/**
 * @brief Perform the marginal effects transition step
 */
AnyType
margins_coxph_int_transition::run(AnyType &args) {

    MarginsCoxPropHazardState<MutableArrayHandle<double> > state = args[0];

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

    // beta is the coefficient vector from cox proportional hazards
    MappedColumnVector beta = args[2].getAs<MappedColumnVector>();


    // basis_indices represents the indices (of beta) for which we want to
    // compute the marginal effect. We don't need to compute the ME for all
    // variables, thus basis_indices could be a subset of all indices.
    MappedColumnVector basis_indices = args[4].getAs<MappedColumnVector>();

    // all variable symbols correspond to the design document
    const uint16_t N = static_cast<uint16_t>(beta.size());
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
             throw std::runtime_error("The categorical indices contain NULL values");
        }
        numCategoricalVars = static_cast<uint16_t>(categorical_indices.size());
    }

    if (state.numRows == 0) {
        if (f.size() > std::numeric_limits<uint16_t>::max())
            throw std::domain_error("Number of independent variables cannot be "
                                    "larger than 65535.");

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
        state.reset();

        // coxph stores the Hessian matrix in it's output table. This is
        // different from the various regression methods that provides the
        // training vcov matrix directly.
        Matrix hessian = args[3].getAs<MappedMatrix>();
        // Computing pseudo inverse of a PSD matrix
        SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
            hessian, EigenvaluesOnly, ComputePseudoInverse);
        state.training_data_vcov = decomposition.pseudoInverse();

        if (state.numCategoricalVarsInSubset > 0){
            for (int i=0; i < state.numCategoricalVarsInSubset; ++i){
                state.categorical_basis_indices(i) = tmp_cat_basis_indices[i];
            }
        }
    }

    // Now do the transition step
    state.numRows++;
    double f_beta = dot(f, beta);
    state.baseline_hazard += f_beta;
    double exp_f_beta = exp(f_beta);

    // compute marginal effects and delta using 1st and 2nd derivatives
    ColumnVector J_trans_beta;
    J_trans_beta = trans(J) * beta;
    ColumnVector curr_margins = exp_f_beta * J_trans_beta;
    Matrix curr_delta = exp_f_beta * (trans(J) + J_trans_beta * trans(f));

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
        double h_set = exp(dot(f_set, beta));
        double h_unset = exp(dot(f_unset, beta));

        curr_margins(static_cast<uint16_t>(state.categorical_basis_indices(i))) = h_set - h_unset;
        curr_delta.row(static_cast<uint16_t>(state.categorical_basis_indices(i))) =
            (h_set * trans(f_set) - h_unset * trans(f_unset));
    }

    state.marginal_effects += curr_margins;
    state.delta += curr_delta;
    return state;
}


/**
 * @brief Cox Proportional Hazards marginal effects: Merge transition states
 */
AnyType
margins_coxph_int_merge::run(AnyType &args) {
    MarginsCoxPropHazardState<MutableArrayHandle<double> > stateLeft = args[0];
    MarginsCoxPropHazardState<ArrayHandle<double> > stateRight = args[1];
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
 * @brief Cox Proportional Hazards marginal effects: Final step
 */
AnyType
margins_coxph_int_final::run(AnyType &args) {
    // We request a mutable object.
    // Depending on the backend, this might perform a deep copy.
    MarginsCoxPropHazardState<MutableArrayHandle<double> > state = args[0];
    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    state.baseline_hazard = exp(state.baseline_hazard / static_cast<double>(state.numRows));
    state.baseline_hazard = 1;
    state.marginal_effects /= static_cast<double>(state.numRows) / state.baseline_hazard;

    // Variance for marginal effects according to the delta method
    Matrix variance;
    variance = state.delta * state.training_data_vcov;

    // we only need the diagonal elements of the variance, so we perform a dot
    // product of each row with state.delta to compute each diagonal element.
    // We divide by numRows^2 since we need the average variance
    ColumnVector std_err =
        variance.cwiseProduct(state.delta).rowwise().sum();
    std_err = std_err.array().sqrt() / static_cast<double>(state.numRows);

    MutableNativeColumnVector tStats(this->allocateArray<double>(state.numBasis));
    MutableNativeColumnVector pValues(this->allocateArray<double>(state.numBasis));

    for (Index i = 0; i < state.numBasis; ++i) {
        tStats(i) = state.marginal_effects(i) / std_err(i);

        // p-values only make sense if numRows > coef.size()
        if (state.numRows > state.numBasis)
            pValues(i) = 2. * prob::cdf( prob::normal(),
                                         -std::abs(tStats(i)));
    }

    // Return all coefficients, standard errors, etc. in a tuple
    // Note: p-values will return NULL if numRows <= coef.size
    AnyType tuple;
    tuple << state.marginal_effects
          << std_err
          << tStats
          << (state.numRows > state.numBasis ? pValues: Null());
    return tuple;
}


/**
 * @brief Cox Proportional Hazards marginal effects: Statistics function
 */
AnyType
margins_compute_stats::run(AnyType &args) {
    // NULL input returns a NULL output
    if (args[0].isNull() || args[1].isNull()) {
        return Null();
    }

    MappedColumnVector marginal_effects = args[0].getAs<MappedColumnVector>();
    MappedColumnVector std_err = args[1].getAs<MappedColumnVector>();
    uint16_t n_basis_terms = static_cast<uint16_t>(marginal_effects.size());
    MutableNativeColumnVector tStats(
        (*this).allocateArray<double>(n_basis_terms));
    MutableNativeColumnVector pValues(
        (*this).allocateArray<double>(n_basis_terms));

    for (Index i = 0; i < n_basis_terms; ++i) {
        tStats(i) = marginal_effects(i) / std_err(i);
        pValues(i) = 2. * prob::cdf( prob::normal(),
                                         -std::abs(tStats(i)));
    }

    // Return all standard errors and p_values in a tuple
    AnyType tuple;
    tuple << marginal_effects << std_err << tStats << pValues;
    return tuple;
}
// ----------------- End of Marginal Effects for Cox proportional hazards ------

} // namespace stats
} // namespace modules
} // namespace madlib
