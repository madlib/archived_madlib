// ---------------------------------------------------------------------------
//             Marginal Effects Multi-Logistic
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

#include <dbconnector/dbconnector.hpp>

#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>
#include <boost/math/distributions.hpp>
#include <modules/prob/student.hpp>
#include "mlogr_margins.hpp"

#include <vector>
#include <fstream>

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

namespace regress {

template <class Handle>
class mlogregrMarginalTransitionState {

    // By §14.5.3/9: "Friend declarations shall not declare partial
    // specializations." We do access protected members in operator+=().
    template <class OtherHandle>
    friend class mlogregrMarginalTransitionState;

  public:
    mlogregrMarginalTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {
        rebind(static_cast<uint16_t>(mStorage[0]),static_cast<uint16_t>(mStorage[1]));
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
     * @brief Initialize the iteratively-reweighted-least-squares state.
     *
     * This function is only called for the first iteration, for the first row.
     */
    inline void initialize(const Allocator &inAllocator,
                           uint16_t inWidthOfX, uint16_t inNumCategories, uint16_t inRefCategory) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                                             dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX, inNumCategories));
        rebind(inWidthOfX, inNumCategories);
        widthOfX = inWidthOfX;
        numCategories = inNumCategories;
        ref_category = inRefCategory;
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle> mlogregrMarginalTransitionState &operator=(
        const mlogregrMarginalTransitionState<OtherHandle> &inOtherState) {
        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle> mlogregrMarginalTransitionState &operator+=(
        const mlogregrMarginalTransitionState<OtherHandle> &inOtherState) {
        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                                   "states");

        numRows += inOtherState.numRows;
        margins_matrix += inOtherState.margins_matrix;
        X_bar += inOtherState.X_bar;
        X_transp_AX += inOtherState.X_transp_AX;
        reference_margins+= inOtherState.reference_margins;
        delta += inOtherState.delta;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        margins_matrix.fill(0);
        X_bar.fill(0);
        X_transp_AX.fill(0);
        reference_margins.fill(0);
        delta.fill(0);
    }

  private:
    static inline uint32_t arraySize(const uint16_t inWidthOfX,
                                     const uint16_t inNumCategories) {
        return 4 + 2*inWidthOfX * inNumCategories + 2*inWidthOfX +
            2 * inNumCategories * inWidthOfX * inWidthOfX * inNumCategories;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX             The number of independent variables.
     * @param inNumCategories The number of categories of the dependant var


     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: widthOfX (number of independant variables)
     * - 1: numCategories (number of categories)
     * - 2: ref_category
     * - 3: coef (vector of coefficients)
     *
     * Intra-iteration components (updated in transition step):
     * - 3 + widthOfX*numCategories: numRows (number of rows already processed in this iteration)
     * - 4 + widthOfX * inNumCategories: margins_matrix
     */
    void rebind(uint16_t inWidthOfX = 0, uint16_t inNumCategories = 0) {
        widthOfX.rebind(&mStorage[0]);
        numCategories.rebind(&mStorage[1]);
        ref_category.rebind(&mStorage[2]);
        coef.rebind(&mStorage[3], inWidthOfX * inNumCategories);
        numRows.rebind(&mStorage[3 + inWidthOfX * inNumCategories]);
        margins_matrix.rebind(&mStorage[4 + inWidthOfX * inNumCategories],
                              inNumCategories , inWidthOfX );
        X_bar.rebind(&mStorage[4 + 2*inWidthOfX * inNumCategories], inWidthOfX);
        reference_margins.rebind(&mStorage[4 + inWidthOfX + 2*inWidthOfX*inNumCategories],
                                 inWidthOfX);
        X_transp_AX.rebind(&mStorage[4 + 2*inWidthOfX * inNumCategories + 2*inWidthOfX],
                           inNumCategories*inWidthOfX, inWidthOfX*inNumCategories);
        delta.rebind(&mStorage[4 + 2*inWidthOfX * inNumCategories + 2*inWidthOfX +
                               inNumCategories * inWidthOfX * inWidthOfX * inNumCategories],
                     inNumCategories*inWidthOfX, inWidthOfX*inNumCategories);
    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToUInt16 numCategories;

    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap margins_matrix;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap X_transp_AX;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap reference_margins;
    typename HandleTraits<Handle>::ReferenceToUInt16 ref_category;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap X_bar;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap delta;
};

// ----------------------------------------------------------------------

AnyType
mlogregr_marginal_step_transition::run(AnyType &args) {
	mlogregrMarginalTransitionState<MutableArrayHandle<double> > state = args[0];

    if (args[1].isNull() || args[2].isNull() || args[3].isNull() ||
        args[4].isNull() || args[5].isNull()) {
        return args[0];
    }

    // Get x as a vector of double
    MappedColumnVector x;
    try{
        // an exception is raised in the backend if args[2] contains nulls
        MappedColumnVector xx = args[4].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    // Get the category & numCategories as integer
    int16_t category = args[1].getAs<int>();
    // Number of categories after pivoting (We pivot around ref_category)
    int16_t numCategories = (args[2].getAs<int>() - 1);
    int32_t ref_category = args[3].getAs<int32_t>();
	MappedMatrix coefMat = args[5].getAs<MappedMatrix>();

    // The following check was added with MADLIB-138.
    if (!x.is_finite())
        throw std::domain_error("Design matrix is not finite.");

    if (state.numRows == 0) {
        if (x.size() > std::numeric_limits<uint16_t>::max())
            throw std::domain_error("Number of independent variables cannot be "
                                    "larger than 65535.");
        if (numCategories < 1)
            throw std::domain_error("Number of cateogires must be at least 2");
        if (category > numCategories)
            throw std::domain_error("You have entered a category > numCategories"
                                    "Categories must be of values {0,1... numCategories-1}");

        // Init the state (requires x.size() and category.size())
        state.initialize(*this,
                         static_cast<uint16_t>(x.size()) ,
                         static_cast<uint16_t>(numCategories),
                         static_cast<uint16_t>(ref_category));

        Matrix mat = coefMat;
        mat.transposeInPlace();
        mat.resize(coefMat.size(), 1);
        state.coef = mat;
    }

    // Now do the transition step
    state.numRows++;
    /*
      Get y: Convert to 1/0 boolean vector
      Example: Category 4 : 0 0 0 1 0 0
      Storing it in this forms helps us get a nice closed form expression
    */

    //To pivot around the specified reference category
    ColumnVector y(numCategories);
    y.fill(0);
    if (category > ref_category) {
        y(category - 1) = 1;
    } else if (category < ref_category) {
        y(category) = 1;
    }


    //    Marginal Effect calculations
    // ----------------------------------------------------------------------
    Matrix coef = state.coef;
    coef.resize(numCategories, state.widthOfX);

    // prob is vector of size # categories
    /*
      Note: The above 2 lines could have been written as:
      ColumnVector prob = -coef*x; but this creates warnings.
      See multilog for details.
    */
    ColumnVector prob(numCategories);
    prob = coef*x;

    // Calculate the odds ratio
    prob = prob.array().exp();
    double prob_sum = prob.sum();
    prob = prob / (1 + prob_sum);

    Matrix probDiag = prob.asDiagonal();
    Matrix a(numCategories,numCategories);
    a = prob * prob.transpose() - probDiag;

    // Start the Hessian calculations
    Matrix X_transp_AX(numCategories * state.widthOfX, numCategories * state.widthOfX);

    /*
      Again: The following 3 lines could have been written as
      Matrix XXTrans = x * x.transpose();
      but it creates warnings related to the type of x. Here is an easy fix
    */
    Matrix cv_x = x;
    Matrix XXTrans = trans(cv_x);
    XXTrans = cv_x * XXTrans;

    //Eigen doesn't supported outer-products for matrices, so we have to do our own.
    //This operation is also known as a tensor-product.
    for (int i1 = 0; i1 < state.widthOfX; i1++){
        for (int i2 = 0; i2 <state.widthOfX; i2++){
            int rowOffset = numCategories * i1;
            int colOffset = numCategories * i2;

            X_transp_AX.block(rowOffset, colOffset, numCategories,  numCategories) = XXTrans(i1, i2) * a;
        }
    }

    triangularView<Lower>(state.X_transp_AX) += X_transp_AX;
    int numIndepVars = state.coef.size() / state.numCategories;

    // Marginal effects (reference calculated separately)
    ColumnVector coef_trans_prob;
    coef_trans_prob = coef.transpose() * prob;
    Matrix margins_matrix = coef;
    margins_matrix.rowwise() -= coef_trans_prob.transpose();
    margins_matrix = prob.asDiagonal() * margins_matrix;

    int idx, index, e_j_J, e_k_K, e_k_K_j_J;
    for (int K=0; K < numIndepVars; K++){
        for (int J=0; J < numCategories; J++){
            idx = K*numCategories + J;
            for (int k=0; k < numIndepVars; k++){
                for (int j=0; j < numCategories; j++){
                    e_j_J = (j==J) ? 1: 0;
                    e_k_K = (k==K) ? 1: 0;
                    e_k_K_j_J = (j==J && k==K) ? 1: 0;

                    index = k*numCategories + j;
                    state.delta(index, idx) +=
                        x(K) * (e_j_J  - prob(J)) * margins_matrix(j,k);
                    state.delta(index, idx) += prob(j) *
                        (e_k_K_j_J - prob(J) * e_k_K - x(K) * margins_matrix(J,k));
                }
            }      
        }
    }

    state.margins_matrix += margins_matrix;

    return state;
}

// ----------------------------------------------------------------------

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 This merge step is the same as that of logistic regression.
*/
AnyType
mlogregr_marginal_step_merge_states::run(AnyType &args) {
    mlogregrMarginalTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    mlogregrMarginalTransitionState<ArrayHandle<double> > stateRight = args[1];

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

AnyType mlogregr_marginalstateToResult(
    const Allocator &inAllocator,
    const int numRows,
    const ColumnVector &inCoef,
    const ColumnVector &inMargins,
    const ColumnVector &inVariance
                                       ) {
    MutableNativeColumnVector margins(
        inAllocator.allocateArray<double>(inMargins.size()));
    MutableNativeColumnVector coef(
        inAllocator.allocateArray<double>(inMargins.size()));
    MutableNativeColumnVector stdErr(
        inAllocator.allocateArray<double>(inMargins.size()));
    MutableNativeColumnVector tStats(
        inAllocator.allocateArray<double>(inMargins.size()));
    MutableNativeColumnVector pValues(
        inAllocator.allocateArray<double>(inMargins.size()));

    for (Index i = 0; i < inMargins.size(); ++i) {
        margins(i) = inMargins(i);
        coef(i) = inCoef(i);
        stdErr(i) = std::sqrt(inVariance(i));
        tStats(i) = margins(i) / stdErr(i);

        // P-values only make sense if numRows > coef.size()
        if (numRows > inCoef.size())
            pValues(i) = 2. * prob::cdf( prob::normal(),
                                         -std::abs(tStats(i)));
    }

    // Return all coefficients, standard errors, etc. in a tuple
    // Note: PValues will return NULL if numRows <= coef.size
    AnyType tuple;
    tuple << margins
          << coef
          << stdErr
          << tStats
          << (numRows > inCoef.size()? pValues: Null());
    return tuple;
}

// ----------------------------------------------------------------------

/**
 * @brief Perform the logistic-regression final step
 */
AnyType
mlogregr_marginal_step_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    mlogregrMarginalTransitionState<ArrayHandle<double> > state = args[0];
    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    if(!state.coef.is_finite())
        throw NoSolutionFoundException("Over- or underflow in Newton step, "
                                       "while updating coefficients. Input data is likely of poor "
                                       "numerical condition.");

    // See MADLIB-138. At least on certain platforms and with certain versions,
    // LAPACK will run into an infinite loop if pinv() is called for non-finite
    // matrices. We extend the check also to the dependent variables.
    if (!state.X_transp_AX.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
                                       "calulation. Input data is likely of poor numerical condition.");

    // Include marginal effects of reference variable:
    // FIXME: They have been taken out of the output for now
    //const int size = state.coef.size() + numIndepVars;
    const int size = state.coef.size();

    // Variance-covariance calculation
    // ----------------------------------------------------------
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        -1 * state.X_transp_AX, EigenvaluesOnly, ComputePseudoInverse);

    // Precompute -(X^T * A * X)^-1
    Matrix V = decomposition.pseudoInverse();

    int numIndepVars = state.coef.size() / state.numCategories;
    int numCategories = state.numCategories;

    Matrix coef = state.coef;
    coef.resize(state.numCategories, state.widthOfX);

    // Variance & Marginal gradient
    ColumnVector marginal_gradient(size);
    ColumnVector variance(size);
    variance.setOnes();

    variance = (state.delta * V * trans(state.delta) / (state.numRows*state.numRows)).diagonal();
    
    // Add in reference variables to all the calculations
    // ----------------------------------------------------------
    ColumnVector coef_with_ref(size);
    ColumnVector margins_with_ref(size);

    // Vectorize the margins_matrix and add the reference variable
    for (int k=0; k < numIndepVars; k++){
        for (int j=0; j < numCategories; j++){
            int index = k * numCategories + j;
            coef_with_ref(index) = coef(j,k);
            margins_with_ref(index) = state.margins_matrix(j,k) / state.numRows;
        }
    }

    return mlogregr_marginalstateToResult(*this,
                                          state.numRows,
                                          coef_with_ref,
                                          margins_with_ref,
                                          variance);
}

}
}
}
