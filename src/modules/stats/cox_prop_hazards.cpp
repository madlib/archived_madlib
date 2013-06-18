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
AnyType stateToResult(const Allocator &inAllocator,
    const HandleMap<const ColumnVector, TransparentHandle<double> >& inCoef,
    const ColumnVector &diagonal_of_inverse_of_X_transp_AX,
    double logLikelihood);

// Internal functions
AnyType intermediate_stateToResult(
		const HandleMap<const ColumnVector, TransparentHandle<double> >
												& x,
		const double inTimeDeath,
		const bool inStatus,
		const double inExp_coef_x,
		const HandleMap<const ColumnVector, TransparentHandle<double> >
												& inCoef,
		const HandleMap<const ColumnVector, TransparentHandle<double> >
												& inX_exp_coef_x,
		const HandleMap<const Matrix, TransparentHandle<double> >
												& inX_xTrans_exp_coef_x
		);




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
        return 6 + 3*inWidthOfX + 2*inWidthOfX*inWidthOfX;
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
 * - 4: exp_coef_x
 * - 5: x_exp_coef_x value (Column Vector)
 * - 6: x_xTrans_exp_coef_x value (Matrix)
 * - 7: Previous State
*/

AnyType cox_prop_hazards_step_transition::run(AnyType &args) {

		// Current state, independant variables & dependant variables
		CoxPropHazardsTransitionState<MutableArrayHandle<double> > state = args[0];
    MappedColumnVector x = args[1].getAs<MappedColumnVector>();
    double y = args[2].getAs<double>();
    bool status = args[3].getAs<bool>();
		
		/** Note: These precomputations come from "intermediate_cox_prop_hazards".
			They are precomputed and stored in a temporary table.
		*/
    double exp_coef_x = args[4].getAs<double>();
    MappedColumnVector x_exp_coef_x = args[5].getAs<MappedColumnVector>();
    MappedMatrix x_xTrans_exp_coef_x = args[6].getAs<MappedMatrix>();
		
    if (state.numRows == 0) {
			state.initialize(*this, static_cast<uint16_t>(x.size()));
			
			if (!args[7].isNull()) {
					CoxPropHazardsTransitionState<ArrayHandle<double> > previousState
																																		= args[7];
					state = previousState;
					state.reset();
					
			}
						
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


/**
 * @brief Newton method final step for Cox Proportional Hazards
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

		// First merge all tied times of death for the last column
		state.grad -= state.multiplier*state.H/state.S;
		triangularView<Lower>(state.hessian) -=
						((state.H*trans(state.H))/(state.S*state.S)
											- state.V/state.S)*state.multiplier;
		state.logLikelihood -=  state.multiplier*std::log(state.S);


		// Computing pseudo inverse of a PSD matrix
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.hessian, EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_hessian = decomposition.pseudoInverse();

		// Newton step 
		state.coef += state.hessian.inverse()*state.grad;
		
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

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.hessian, EigenvaluesOnly, ComputePseudoInverse);

    return stateToResult(*this, state.coef,
					 decomposition.pseudoInverse().diagonal(),
					 state.logLikelihood);
}

/**
 * @brief Compute the diagnostic statistics
 *
 */
AnyType stateToResult(
		const Allocator &inAllocator,
		const HandleMap<const ColumnVector, TransparentHandle<double> > &inCoef,
		const ColumnVector &diagonal_of_inverse_of_hessian,
    double logLikelihood) {

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
		
    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << inCoef << logLikelihood << std_err << waldZStats << waldPValues;
		
    return tuple;
}




/**
 * @brief Newton method transition step for Cox Proportional Hazards
 *
 * @param args
 *
 * Arguments (Matched with PSQL wrapped)
 * - 1: x           (Column Vector)
 * - 1: status      (Double)
 * - 2: coef value  (Column Vector)
*/

AnyType intermediate_cox_prop_hazards::run(AnyType &args) {

    MappedColumnVector x = args[0].getAs<MappedColumnVector>();
    bool status = args[1].getAs<bool>();
    MutableNativeColumnVector coef(allocateArray<double>(x.size()));
		
    // The following check was added with MADLIB-138.
    if (!dbal::eigen_integration::isfinite(x))
        throw std::domain_error("Design matrix is not finite.");

		if (x.size() > std::numeric_limits<uint16_t>::max())
				throw std::domain_error("Number of independent variables cannot be "
						"larger than 65535.");

		if (args[2].isNull()) {
			for (int i=0; i<x.size() ; i++)
				coef(i) = 0;
		}
		else {
			coef = args[2].getAs<MappedColumnVector>();
		}
		
		double exp_coef_x;
		MutableNativeColumnVector
			x_exp_coef_x(allocateArray<double>(x.size()));
		MutableNativeMatrix
			x_xTrans_exp_coef_x(allocateArray<double>(x.size(), x.size()));							

		exp_coef_x = std::exp(trans(coef)*x);
		x_exp_coef_x = exp_coef_x*x;
		x_xTrans_exp_coef_x = x * trans(x) * exp_coef_x;
		
    AnyType tuple;
		tuple << x
					<< status
					<< exp_coef_x
					<< x_exp_coef_x
					<< x_xTrans_exp_coef_x;
		
    return tuple;

}


} // namespace stats

} // namespace modules

} // namespace madlib
