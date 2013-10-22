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

#include "cox_prop_hazards.hpp"

namespace madlib {
namespace modules {
namespace stats {

// Use Eigen
using namespace dbal::eigen_integration;
// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

using namespace std;
// -------------------------------------------------------------------------


// Internal functions
AnyType stateToResult(const Allocator &inAllocator,
                      const HandleMap<const ColumnVector, TransparentHandle<double> >& inCoef,
                      const ColumnVector &diagonal_of_inverse_of_hessian,
                      double logLikelihood, const MappedMatrix &inHessian,
                      int num_rows_processed);

/**
 * @brief Transition state for the Cox Proportional Hazards
 *
 * TransitionState encapsulates the transition state during the
 * aggregate functions. To the database, the state is exposed
 * as a single DOUBLE PRECISION array, to the C++ code it is a proper object
 * containing scalars, a vector, and a matrix.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elements are 0.
 */
template <class Handle>
class CoxPropHazardsTransitionState {

    template <class OtherHandle>
    friend class CoxPropHazardsTransitionState;

  public:
    CoxPropHazardsTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[1]));
    }

    /**
     * @brief Convert to backend representation
     *
     * We define this function so that we can use TransitionState in the argument
     * list and as a return type.   */
    inline operator AnyType() const {
        return mStorage;
    }

    /**
     * @brief Initialize the transition state. Only called for first row.
     *
     * @param inAllocator Allocator for the memory transition state. Must fill
     *     the memory block with zeros.
     * @param inWidthOfX Number of independent variables. The first row of data
     *     determines the size of the transition state. This size is a quadratic
     *     function of inWidthOfX.
     */
    inline void initialize(const Allocator &inAllocator, uint16_t inWidthOfX, double * inCoef = 0) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                                             dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX));
        rebind(inWidthOfX);
        widthOfX = inWidthOfX;
        if(inCoef){
            for(uint16_t i = 0; i < widthOfX; i++)
                coef[i] = inCoef[i];
        }
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    CoxPropHazardsTransitionState &operator=(
        const CoxPropHazardsTransitionState<OtherHandle> &inOtherState) {
        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    CoxPropHazardsTransitionState &operator+=(
        const CoxPropHazardsTransitionState<OtherHandle> &inOtherState) {
        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error(
                "Internal error: Incompatible transition states");

        numRows += inOtherState.numRows;
        // coef += inOtherState.coef;
        grad += inOtherState.grad;
        S += inOtherState.S;
        H += inOtherState.H;
        logLikelihood += inOtherState.logLikelihood;
        V += inOtherState.V;
        hessian += inOtherState.hessian;

        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        S = 0;
        y_previous = 0;
        multiplier = 0;
        H.fill(0);
        V.fill(0);
        grad.fill(0);
        hessian.fill(0);
        logLikelihood = 0;

    }

  private:
    static inline size_t arraySize(const uint16_t inWidthOfX) {
        return 6 + 3 * inWidthOfX + 2 * inWidthOfX * inWidthOfX;
    }

    void rebind(uint16_t inWidthOfX) {
        // Inter iteration components
        numRows.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        multiplier.rebind(&mStorage[2]);
        y_previous.rebind(&mStorage[3]);
        coef.rebind(&mStorage[4], inWidthOfX);

        // Intra iteration components
        S.rebind(&mStorage[4+inWidthOfX]);
        H.rebind(&mStorage[5+inWidthOfX], inWidthOfX);
        grad.rebind(&mStorage[5+2*inWidthOfX],inWidthOfX);
        logLikelihood.rebind(&mStorage[5+3*inWidthOfX]);
        V.rebind(&mStorage[6+3*inWidthOfX],
                 inWidthOfX, inWidthOfX);
        hessian.rebind(&mStorage[6+3*inWidthOfX+inWidthOfX*inWidthOfX],
                       inWidthOfX, inWidthOfX);
    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToDouble multiplier;
    typename HandleTraits<Handle>::ReferenceToDouble y_previous;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;

    typename HandleTraits<Handle>::ReferenceToDouble S;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap H;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap grad;
    typename HandleTraits<Handle>::ReferenceToDouble logLikelihood;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap V;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap hessian;
};

// ----------------------------------------------------------------------

/**
 * @brief Newton method transition step for Cox Proportional Hazards
 *
 * @param args
 *
 * Arguments (Matched with PSQL wrapped)
 * - 0: Current State
 * - 1: x
 * - 2: y
 * - 3: status
 * - 4: coef
 */

AnyType coxph_step_transition::run(AnyType &args) {
    // Current state, independant variables & dependant variables
    CoxPropHazardsTransitionState<MutableArrayHandle<double> > state = args[0];
    if (args[1].isNull() || args[2].isNull()) { return args[0]; }

    double y = args[2].getAs<double>();
    bool status;
    if (args[3].isNull()) {
        // by default we assume that the data is uncensored => status = TRUE
        status = true;
    } else {
        status = args[3].getAs<bool>();
    }

    MappedColumnVector x;
    try {
        // an exception is raised in the backend if input data contains nulls
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        // independent variable array contains NULL. We skip this row
        return args[0];
    }

    // The following check was added with MADLIB-138.
    if (!dbal::eigen_integration::isfinite(x))
        throw std::domain_error("Design matrix is not finite.");

    if (x.size() > std::numeric_limits<uint16_t>::max())
        throw std::domain_error(
            "Number of independent variables cannot be larger than 65535.");

    MutableNativeColumnVector coef(allocateArray<double>(x.size()));
    if (args[4].isNull())
        for (int i=0; i<x.size(); i++) coef(i) = 0;
    else
        coef = args[4].getAs<MappedColumnVector>();

    MutableNativeColumnVector x_exp_coef_x(allocateArray<double>(x.size()));
    MutableNativeMatrix x_xTrans_exp_coef_x(
            allocateArray<double>(x.size(), x.size()));
    double exp_coef_x = std::exp(trans(coef)*x);

    x_exp_coef_x = exp_coef_x * x;
    x_xTrans_exp_coef_x = x * trans(x) * exp_coef_x;

    if (state.numRows == 0) {
        state.initialize(*this, static_cast<uint16_t>(x.size()), coef.data());
    }

    state.numRows++;

    /** In case of a tied time of death or in the first iteration:
        We must only perform the "pre compuations". When the tie is resolved
        we add up all the precomputations once in for all. This is
        an implementation of Breslow's method.
        The time of death for two records are considered "equal" if they
        differ by less than 1.0e-6.
        Also, in case status = 0, the observation must be censored so no
        computations are required
    */

    if (std::abs(y-state.y_previous) < 1.0e-6 || state.numRows == 1) {
        if (status == 1) {
            state.multiplier++;
        }
    }
    else {

		/** Resolve the ties by adding all the precomputations once in for all
            Note: The hessian is the negative of the design document because we
            want it to stay PSD (makes it easier for inverse compuations)
		*/
        state.grad -= state.multiplier*state.H/state.S;
        triangularView<Lower>(state.hessian) -=
            ((state.H*trans(state.H))/(state.S*state.S)
             - state.V/state.S)*state.multiplier;
        state.logLikelihood -=  state.multiplier*std::log(state.S);
        state.multiplier = status;

    }

    /** These computations must always be performed irrespective of whether
        there are ties or not.
        Note: See design documentation for details on the implementation.
    */
    state.S += exp_coef_x;
    state.H += x_exp_coef_x;
    state.V += x_xTrans_exp_coef_x;
    state.y_previous = y;
    if (status == 1) {
        state.grad += x;
        state.logLikelihood += std::log(exp_coef_x);
    }
    return state;
}

// ----------------------------------------------------------------------

/**
 * @brief Newton method final step for Cox Proportional Hazards
 *
 */
AnyType coxph_step_final::run(AnyType &args) {
    CoxPropHazardsTransitionState<MutableArrayHandle<double> > state = args[0];

    // If we haven't seen any data, just return Null.
    if (state.numRows == 0)
        return Null();

    if (!state.hessian.is_finite() || !state.grad.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
                                       "calulation. Input data is likely of poor numerical condition.");

    // First merge all tied times of death for the last row
    state.grad -= state.multiplier*state.H/state.S;
    triangularView<Lower>(state.hessian) -=
        ((state.H*trans(state.H))/(state.S*state.S)
         - state.V/state.S)*state.multiplier;
    state.logLikelihood -=  state.multiplier*std::log(state.S);


    if (isinf(static_cast<double>(state.logLikelihood)) || isnan(static_cast<double>(state.logLikelihood))) 
        throw NoSolutionFoundException("Over- or underflow in intermediate "
                                       "calulation. Input data is likely of poor numerical condition.");

    // Computing pseudo inverse of a PSD matrix
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.hessian, EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_hessian = decomposition.pseudoInverse();

    // Newton step
    //state.coef += state.hessian.inverse()*state.grad;
    state.coef += inverse_of_hessian * state.grad;

    // Return all coefficients etc. in a tuple
    return state;
}

// ----------------------------------------------------------------------

/**
 * @brief Return the difference in log-likelihood between two states
 */
AnyType internal_coxph_step_distance::run(AnyType &args) {
    CoxPropHazardsTransitionState<ArrayHandle<double> > stateLeft = args[0];
    CoxPropHazardsTransitionState<ArrayHandle<double> > stateRight = args[1];
    return std::abs(stateLeft.logLikelihood - stateRight.logLikelihood);
}

// ----------------------------------------------------------------------

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType internal_coxph_result::run(AnyType &args) {
    CoxPropHazardsTransitionState<ArrayHandle<double> > state = args[0];

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.hessian, EigenvaluesOnly, ComputePseudoInverse);

    return stateToResult(*this, state.coef,
                         decomposition.pseudoInverse().diagonal(),
                         state.logLikelihood, state.hessian, state.numRows);
}

// ----------------------------------------------------------------------

/**
 * @brief Compute the diagnostic statistics
 *
 */
AnyType stateToResult(
    const Allocator &inAllocator,
    const HandleMap<const ColumnVector, TransparentHandle<double> > &inCoef,
    const ColumnVector &diagonal_of_inverse_of_hessian,
    double logLikelihood, const MappedMatrix &inHessian,
    int num_rows_processed) {

    MutableNativeColumnVector std_err(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector waldZStats(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector waldPValues(
        inAllocator.allocateArray<double>(inCoef.size()));

    for (Index i = 0; i < inCoef.size(); ++i) {
        std_err(i) = std::sqrt(diagonal_of_inverse_of_hessian(i));
        waldZStats(i) = inCoef(i) / std_err(i);
        waldPValues(i) = 2. * prob::cdf( prob::normal(),
                                         -std::abs(waldZStats(i)));
    }

    // Hessian being symmetric is updated as lower triangular matrix.
    // We need to convert diagonal matrix to full-matrix before output
    Matrix full_hessian = inHessian + inHessian.transpose();
    full_hessian.diagonal() /= 2;

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << inCoef << logLikelihood << std_err << waldZStats << waldPValues
          << full_hessian << num_rows_processed;
    return tuple;
}

// ----------------------------------------------------------------------

AnyType coxph_step_outer_transition::run(AnyType &args) {
    if (args[0].isNull()) return args[1];
    if (args[1].isNull()) return args[0];

    CoxPropHazardsTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    CoxPropHazardsTransitionState<ArrayHandle<double> > stateRight = args[1];

    stateLeft += stateRight;
    return stateLeft;
}

// ----------------------------------------------------------------------

/**
 * @brief Newton method final step for Cox Proportional Hazards
 *
 */
AnyType coxph_step_strata_final::run(AnyType &args) {
    CoxPropHazardsTransitionState<MutableArrayHandle<double> > state = args[0];

    // If we haven't seen any data, just return Null.
    if (state.numRows == 0)
        return Null();

    if (!state.hessian.is_finite() || !state.grad.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
                                       "calulation. Input data is likely of poor numerical condition.");

    // Computing pseudo inverse of a PSD matrix
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.hessian, EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_hessian = decomposition.pseudoInverse();

    // Newton step
    //state.coef += state.hessian.inverse()*state.grad;
    state.coef += inverse_of_hessian * state.grad;

    // Return all coefficients etc. in a tuple
    return state;
}

// ----------------------------------------------------------------------

/**
 * @brief Newton method final step for Cox Proportional Hazards
 *
 */
AnyType coxph_step_inner_final::run(AnyType &args) {
    CoxPropHazardsTransitionState<MutableArrayHandle<double> > state = args[0];

    // If we haven't seen any data, just return Null.
    if (state.numRows == 0)
        return Null();

    if (!state.hessian.is_finite() || !state.grad.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
                                       "calulation. Input data is likely of poor numerical condition.");

    // First merge all tied times of death for the last row
    state.grad -= state.multiplier*state.H/state.S;
    triangularView<Lower>(state.hessian) -=
        ((state.H*trans(state.H))/(state.S*state.S)
         - state.V/state.S)*state.multiplier;
    state.logLikelihood -=  state.multiplier*std::log(state.S);

    // Return all coefficients etc. in a tuple
    return state;
}


// -----------------------------------------------------------------------
// Schoenfeld Residual Aggregate
// -----------------------------------------------------------------------
AnyType zph_transition::run(AnyType &args){
    if (args[1].isNull()) { return args[0]; }

    MappedColumnVector x;
    try {
        // an exception is raised in the backend if args[2] contains nulls
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
     } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    if (!dbal::eigen_integration::isfinite(x))
        throw std::domain_error("Design matrix is not finite.");

    int data_dim = x.size();
    if (data_dim > std::numeric_limits<uint16_t>::max())
        throw std::domain_error(
                "Number of independent variables cannot be larger than 65535.");

    // std::ostringstream s;
    // s << "(";
    // for (int i=0; i < x.size(); i++)
    //     s << x[i] << ", ";
    // s << ")";
    // elog(INFO, s.str().c_str());


    MutableArrayHandle<double> state(NULL);
    if (args[0].isNull()){
        // state[0:data_dim-1]  - x * exp(coeff . x)
        // state[data_dim]      - exp(coeff . x)
        state = allocateArray<double>(data_dim + 1);
    } else {
        state = args[0].getAs<MutableArrayHandle<double> >();
    }


    MutableNativeColumnVector coef(allocateArray<double>(data_dim));
    if (args[2].isNull()){
        for (int i=0; i < data_dim ; i++)
            coef(i) = 0;
    } else {
        coef = args[2].getAs<MappedColumnVector>();
    }

    double exp_coef_x = std::exp(trans(coef) * x);
    MutableNativeColumnVector x_exp_coef_x(allocateArray<double>(data_dim));
    x_exp_coef_x = exp_coef_x * x;

    for (int i =0; i < data_dim; i++)
        state[i] += x_exp_coef_x[i];
    state[data_dim] += exp_coef_x;

    return state;
}
// -------------------------------------------------------------------------

AnyType zph_merge::run(AnyType &/* args */){
    throw std::logic_error("The aggregate is used as an aggregate over window. "
                           "The merge function should not be used in this scenario.");
}
// -------------------------------------------------------------------------
AnyType zph_final::run(AnyType &args){
    if (args[0].isNull())
        return Null();

    MutableArrayHandle<double> state(NULL);
    state = args[0].getAs<MutableArrayHandle<double> >();
    size_t data_dim = state.size()-1;

    MutableArrayHandle<double> result = allocateArray<double>(data_dim);
    for (size_t i = 0; i < data_dim; i++)
        result[i] = state[i]/state[data_dim];
    return result;
}

// -----------------------------------------------------------------------
// Correlation aggregate between an array and a scalar
// -----------------------------------------------------------------------
/**
 * @brief Transition state for the Cox Proportional Hazards
 *
 * TransitionState encapsulates the transition state during the
 * aggregate functions. To the database, the state is exposed
 * as a single DOUBLE PRECISION array, to the C++ code it is a proper object
 * containing scalars, a vector, and a matrix.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elements are 0.
 */
template <class Handle>
class ArrayElemCorrState {

    template <class OtherHandle>
    friend class ArrayElemCorrState;

  public:
    ArrayElemCorrState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[1]));
    }

    /**
     * @brief Convert to backend representation
     *
     * We define this function so that we can use TransitionState in the argument
     * list and as a return type.   */
    inline operator AnyType() const {
        return mStorage;
    }

    /**
     * @brief Initialize the transition state. Only called for first row.
     *
     * @param inAllocator Allocator for the memory transition state. Must fill
     *     the memory block with zeros.
     * @param inWidthOfX Number of independent variables. The first row of data
     *     determines the size of the transition state. This size is a quadratic
     *     function of inWidthOfX.
     */
    inline void initialize(const Allocator &inAllocator, uint16_t inWidthOfX) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                                             dbal::DoZero,
                                             dbal::ThrowBadAlloc>(arraySize(inWidthOfX));
        rebind(inWidthOfX);
        widthOfX = inWidthOfX;
        this->reset();
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    ArrayElemCorrState &operator=(
        const ArrayElemCorrState<OtherHandle> &inOtherState) {
        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    ArrayElemCorrState &operator+=(
        const ArrayElemCorrState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error(
                "Internal error: Incompatible transition states");

        numRows += inOtherState.numRows;
        sum_y += inOtherState.sum_y;
        sum_yy += inOtherState.sum_yy;
        sum_x += inOtherState.sum_x;
        sum_xx += inOtherState.sum_xx;
        sum_xy += inOtherState.sum_xy;

        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        sum_y = 0;
        sum_yy = 0;
        sum_xy.fill(0);
        sum_x.fill(0);
        sum_xx.fill(0);
    }

  private:
    static inline size_t arraySize(const uint16_t inWidthOfX) {
        return 4 + 3 * inWidthOfX;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX The number of independent variables.
     *
     * Array layout:
     * Inter iteration components (updated in the final step)
     * - 0: numRows (number of rows seen so far)
     * - 1: widthOfX (number of features)
     * - 2: coef (multipliers for each of the features)

     * Intra interation components (updated in the current interation)
     * - 2 + widthofX: S (see design document for details)
     * - 3 + widthofX: Hi[j] (see design document for details)
     * - 3 + 2*widthofX: gradCoef (coefficients of the gradient)
     * - 3 + 3*widthofX: logLikelihood
     * - 4 + 3*widthofX: V (Precomputations for the hessian)
     * - 4 + 3*widthofX + widthofX^2: hessian
     *
     */
    void rebind(uint16_t inWidthOfX) {
        // Inter iteration components
        numRows.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        sum_y.rebind(&mStorage[2]);
        sum_yy.rebind(&mStorage[3]);

        // Intra iteration components
        sum_xy.rebind(&mStorage[4], inWidthOfX);
        sum_x.rebind(&mStorage[4+inWidthOfX], inWidthOfX);
        sum_xx.rebind(&mStorage[4+2*inWidthOfX], inWidthOfX);
    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToDouble sum_y;
    typename HandleTraits<Handle>::ReferenceToDouble sum_yy;

    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap sum_xy;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap sum_x;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap sum_xx;
};

AnyType array_elem_corr_transition::run(AnyType &args){

    ArrayElemCorrState<MutableArrayHandle<double> > state = args[0];

    if (args[1].isNull() || args[2].isNull()) { return args[0]; }

    MappedColumnVector x;
    try {
        // an exception is raised in the backend if input data contains nulls
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    double y = args[2].getAs<double>();

    if (!dbal::eigen_integration::isfinite(x))
        throw std::domain_error("Design matrix is not finite.");

    if (x.size() > std::numeric_limits<uint16_t>::max())
        throw std::domain_error(
            "Number of variables cannot be larger than 65535.");

    if (state.numRows == 0) {
        state.initialize(*this, static_cast<uint16_t>(x.size()));
    }

    state.numRows += 1;
    state.sum_y += y;
    state.sum_yy += y*y;
    state.sum_x += x;
    state.sum_xx += x.cwiseProduct(x);  // perform element-wise multiplication
    state.sum_xy += x * y;  // perform scalar multiplication

    return state;
}

AnyType array_elem_corr_merge::run(AnyType &args) {
    ArrayElemCorrState<MutableArrayHandle<double> > stateLeft = args[0];
    ArrayElemCorrState<ArrayHandle<double> > stateRight = args[1];

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

AnyType array_elem_corr_final::run(AnyType &args) {
    ArrayElemCorrState<MutableArrayHandle<double> > state = args[0];

    // If we haven't seen any data, just return Null.
    if (state.numRows == 0)
        return Null();

    ColumnVector S_xy =
        state.numRows * state.sum_xy - state.sum_x  * state.sum_y;
    ColumnVector S_xx =
        state.numRows * state.sum_xx - state.sum_x.cwiseProduct(state.sum_x);
    double S_yy = state.numRows * state.sum_yy - state.sum_y * state.sum_y;
    ColumnVector correlation = S_xy.cwiseQuotient(S_xx.cwiseSqrt() * sqrt(S_yy));

    return correlation;
}

AnyType coxph_resid_stat_transition::run(AnyType &args) {
    double w = args[1].getAs<double>();
    ArrayHandle<double> residual = args[2].getAs<ArrayHandle<double> >();
    ArrayHandle<double> hessian = args[3].getAs<ArrayHandle<double> >();
    int m = args[4].getAs<int>();

    MutableArrayHandle<double> state(NULL);
    if(args[0].isNull()){
        int n = residual.size();
        // state[0]: m
        // state[1]: n
        // state[2]: w_t * w
        // state[3:2 + n]: w_t * residual  (size = n)
        // state[n + 3:n + n * n  + 2]: hessian (size = n*n)
        state = allocateArray<double>(n * n + n + 3);
        state[0] = m;
        state[1] = n;
        for(size_t i = 0; i < hessian.size(); i++)
            state[n + 3 + i] = hessian[i];
    }else{
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    state[2] += w * w;
    for(size_t i = 0; i < residual.size(); i++)
        state[3 + i] += residual[i] * w;

    return state;
}

AnyType coxph_resid_stat_merge::run(AnyType &args) {
    if(args[0].isNull())
        return args[1];
    if(args[1].isNull())
        return args[0];

    MutableArrayHandle<double> state1 = args[0].getAs<MutableArrayHandle<double> >();
    ArrayHandle<double> state2 = args[1].getAs<ArrayHandle<double> >();
    int n = static_cast<int>(state1[1]);

    for(int i = 2; i <= n + 2; i++)
        state1[i] += state2[i];

    return state1;
}

AnyType coxph_resid_stat_final::run(AnyType &args) {
    if(args[0].isNull())
        return Null();

    using boost::math::complement;
    MutableArrayHandle<double> state = args[0].getAs<MutableArrayHandle<double> >();
    int m = static_cast<int>(state[0]);
    int n = static_cast<int>(state[1]);
    double w_trans_w = state[2];

    // std::ostringstream out_str;
    // out_str << "m = " << m << " n = " << n;
    // elog(INFO, out_str.str().c_str());

    Eigen::Map<Matrix> w_trans_residual(state.ptr() + 3, 1, n);
    Eigen::Map<Matrix> hessian(state.ptr() + 3 + n, n, n);

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        hessian, EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_hessian = decomposition.pseudoInverse();

    ColumnVector v =  m * (w_trans_residual * inverse_of_hessian).transpose();
    ColumnVector v_v = v.cwiseProduct(v);

    ColumnVector covar_diagonal = inverse_of_hessian.diagonal();
    ColumnVector z = v_v.cwiseQuotient(covar_diagonal * m * w_trans_w);
    ColumnVector p(n);
    for(int i = 0; i < n; i++)
        p(i) = prob::cdf(complement(prob::chi_squared(static_cast<double>(1)), z(i)));

    AnyType tuple;
    tuple << z << p;
    return tuple;
}

AnyType coxph_scale_resid::run(AnyType &args) {
    int m = args[0].getAs<int>();
    MappedMatrix hessian = args[1].getAs<MappedMatrix>();
    MappedColumnVector residual = args[2].getAs<MappedColumnVector>();

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        hessian, EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_hessian = decomposition.pseudoInverse();

    ColumnVector scaled_residual = (inverse_of_hessian * Matrix(residual)) * m;
    return scaled_residual;
}

} // namespace stats
} // namespace modules
} // namespace madlib
