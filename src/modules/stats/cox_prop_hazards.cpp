/* ----------------------------------------------------------------------- *//**
 *
 * @file cox_proportional_hazards.cpp
 *
 * @brief Cox proportional hazards
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>

#include "cox_prop_hazards.hpp"


namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

namespace stats {

// Internal functions
AnyType stateToResult(
	const HandleMap<const ColumnVector, TransparentHandle<double> >& inCoef,
	double logLikelihood);



/**
 * @brief Transition state for the Cox Proportional Hazards
 *
 * TransitionState encapsulates the transition state during the
 * linear-regression aggregate functions. To the database, the state is exposed
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
    inline void initialize(const Allocator &inAllocator, uint16_t inWidthOfX) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX));
        rebind(inWidthOfX);
        widthOfX = inWidthOfX;
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
            throw std::logic_error("Internal error: Incompatible transition "
                "states");

        numRows += inOtherState.numRows;
        coef += inOtherState.coef;
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
        H.fill(0);
        V.fill(0);				
        grad.fill(0);
        hessian.fill(0);
        logLikelihood = 0;

    }


private:
    static inline size_t arraySize(const uint16_t inWidthOfX) {
        return 4 + 3*inWidthOfX + 2*inWidthOfX*inWidthOfX;
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
        numRows.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        coef.rebind(&mStorage[2], inWidthOfX);
		
        S.rebind(&mStorage[2+inWidthOfX]);
        H.rebind(&mStorage[3+inWidthOfX], inWidthOfX);
        grad.rebind(&mStorage[3+2*inWidthOfX],inWidthOfX);
				logLikelihood.rebind(&mStorage[3+3*inWidthOfX]);
				V.rebind(&mStorage[4+3*inWidthOfX],
					inWidthOfX, inWidthOfX);
				hessian.rebind(&mStorage[4+3*inWidthOfX+inWidthOfX*inWidthOfX],
					inWidthOfX, inWidthOfX);
				

    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
		typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
		
    typename HandleTraits<Handle>::ReferenceToDouble S;
    typename HandleTraits<Handle>::ReferenceToDouble logLikelihood;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap H;
		typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap grad;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap V;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap hessian;

};


/**
 * @brief Gradient descent transition step for Cox Proportional Hazards
 *
 * @param args
 *
 * Arguments (Matched with PSQL wrapped)
 * - 0: Current State
 * - 1: X value (Column Vector)
 * - 2: Previous State
*/

AnyType cox_prop_hazards_step_transition::run(AnyType &args) {

    CoxPropHazardsTransitionState<MutableArrayHandle<double> > state = args[0];
    MappedColumnVector x = args[1].getAs<MappedColumnVector>();

    // The following check was added with MADLIB-138.
    if (!isfinite(x)){
        throw std::domain_error("Design matrix is not finite.");
		}

    // Now do the transition step.
    if (state.numRows == 0) {
        if (x.size() > std::numeric_limits<uint16_t>::max())
            throw std::domain_error("Number of independent variables cannot be "
                "larger than 65535.");

        state.initialize(*this, static_cast<uint16_t>(x.size()));
        if (!args[2].isNull()) {
            CoxPropHazardsTransitionState<ArrayHandle<double> > previousState = args[2];
            state = previousState;
            state.reset();
        }

    }

    state.numRows++;
		
		double xc = trans(state.coef)*x;
		double s = std::exp(xc);
		
		state.S += s;
		state.H += s*x;		
		state.V += x * trans(x) * s;

		state.grad += x - state.H/state.S;
		state.hessian += (state.H * trans(state.H))/(state.S*state.S) - state.V/state.S;
		state.logLikelihood += xc - std::log(state.S);
		
		
    return state;
}


/**
 * @brief Gradient descent final step for Cox Proportional Hazards
 *
 */
AnyType cox_prop_hazards_step_final::run(AnyType &args) {

    CoxPropHazardsTransitionState<MutableArrayHandle<double> > state = args[0];

    // If we haven't seen any data, just return Null. 
    if (state.numRows == 0)
        return Null();


    if (!state.hessian.is_finite() || !state.grad.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
            "calulation. Input data is likely of poor numerical condition.");

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.hessian, EigenvaluesOnly, ComputePseudoInverse);
		Matrix inverse_of_hessian = decomposition.pseudoInverse();

		// Fixed Step size of 1		
		state.coef = state.coef - state.hessian.inverse()*state.grad;
    // Return all coefficients etc. in a tuple
    return state;
}

/**
 * @brief Return the difference in log-likelihood between two states
 */
AnyType internal_cox_prop_hazards_step_distance::run(AnyType &args) {
    CoxPropHazardsTransitionState<ArrayHandle<double> > stateLeft = args[0];
    CoxPropHazardsTransitionState<ArrayHandle<double> > stateRight = args[1];

		
    return std::abs(stateLeft.logLikelihood - stateRight.logLikelihood);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType internal_cox_prop_hazards_result::run(AnyType &args) {

    CoxPropHazardsTransitionState<ArrayHandle<double> > state = args[0];
    return stateToResult(state.coef, state.logLikelihood);
}

/**
 * @brief Compute the diagnostic statistics
 *
 */
AnyType stateToResult(
		const HandleMap<const ColumnVector, TransparentHandle<double> > &inCoef,
		double logLikelihood) {

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
		tuple << inCoef << logLikelihood;
		
    return tuple;
}


} // namespace stats

} // namespace modules

} // namespace madlib
