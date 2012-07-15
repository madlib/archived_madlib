
/* ----------------------------------------------------------------------- *//**
 *
 * @file multilogistic.cpp
 *
 * @brief Multinomial Logistic-Regression functions
 *
 * We implement the iteratively-reweighted-least-squares method.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>
#include "multilogistic.hpp"

#include <vector>
#include <fstream>
 
namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

namespace regress {


// Internal functions
AnyType mLogstateToResult(const Allocator &inAllocator,
    const HandleMap<const ColumnVector, TransparentHandle<double> >& inCoef,
    const ColumnVector &diagonal_of_inverse_of_X_transp_AX,
    double logLikelihood,
    double conditionNo);


/**
 * @brief Logistic function
 */
inline double sigma(double x) {
	return 1. / (1. + std::exp(-x));
}


/**
 * @brief Inter- and intra-iteration state for iteratively-reweighted-least-
 *        squares method for logistic regression
 *
 * TransitionState encapsualtes the transition state during the
 * logistic-regression aggregate function. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars, a vector, and a matrix.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 4, and all elemenets are 0.
 */
template <class Handle>
class MLogRegrIRLSTransitionState {

    // By §14.5.3/9: "Friend declarations shall not declare partial
    // specializations." We do access protected members in operator+=().
    template <class OtherHandle>
    friend class MLogRegrIRLSTransitionState;

public:


    MLogRegrIRLSTransitionState(const AnyType &inArray)
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
    inline void initialize(const Allocator &inAllocator
													, uint16_t inWidthOfX, uint16_t inNumCategories) {
		
		
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX, inNumCategories));
        rebind(inWidthOfX, inNumCategories);
        widthOfX = inWidthOfX;
				numCategories = inNumCategories;
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    MLogRegrIRLSTransitionState &operator=(
        const MLogRegrIRLSTransitionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    MLogRegrIRLSTransitionState &operator+=(
        const MLogRegrIRLSTransitionState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");

        numRows += inOtherState.numRows;
        X_transp_Az += inOtherState.X_transp_Az;
        X_transp_AX += inOtherState.X_transp_AX;
        logLikelihood += inOtherState.logLikelihood;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        X_transp_Az.fill(0);
        X_transp_AX.fill(0);
        logLikelihood = 0;
    }

private:
    static inline uint32_t arraySize(const uint16_t inWidthOfX, 
																			const uint16_t inNumCategories) {
        return 4 + inWidthOfX * inWidthOfX * inNumCategories * inNumCategories
								 + 2 * inWidthOfX * inNumCategories;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX 			The number of independent variables.
     * @param inNumCategories The number of categories of the dependant var
		 
		 
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: widthOfX (number of independant variables)
     * - 1: numCategories (number of categories)
     * - 2: coef (vector of coefficients)
     *
     * Intra-iteration components (updated in transition step):
     * - 2 + widthOfX*numCategories: numRows (number of rows already processed in this iteration)
     * - 3 + widthOfX*numCategories: X_transp_Az (X^T A z)
     * - 3 + 2 * widthOfX * inNumCategories: X_transp_AX (X^T A X)
     * - 3 + widthOfX^2*numCategories^2 
		 				+ 2 * widthOfX*numCategories: logLikelihood ( ln(l(c)) )
     */
    void rebind(uint16_t inWidthOfX = 0, uint16_t inNumCategories = 0) {
				
        widthOfX.rebind(&mStorage[0]);				
        numCategories.rebind(&mStorage[1]);				
        coef.rebind(&mStorage[2], inWidthOfX*inNumCategories);				
				
        numRows.rebind(&mStorage[2 + inWidthOfX*inNumCategories]);
				
        X_transp_Az.rebind(&mStorage[3 + inWidthOfX*inNumCategories],
																		 inWidthOfX*inNumCategories);
        X_transp_AX.rebind(&mStorage[3 + 2 * inWidthOfX*inNumCategories], 
																		inNumCategories*inWidthOfX, 
																		inWidthOfX*inNumCategories);
        logLikelihood.rebind(
					&mStorage[3 + inNumCategories*inNumCategories*inWidthOfX*inWidthOfX 
											+ 2 * inWidthOfX*inNumCategories]);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToUInt16 numCategories;

    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap X_transp_Az;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap X_transp_AX;
    typename HandleTraits<Handle>::ReferenceToDouble logLikelihood;
};



/**
 * @brief IRLS Transition
 * @param args
 *
 * Arguments (Matched with PSQL wrapped)
 * - 0: Current State
 * - 1: y value (Integer)
 * - 2: numCategories (Integer)
 * - 3: X value (Column Vector)
 * - 4: Previous State

 */
AnyType
mlogregr_irls_step_transition::run(AnyType &args) {


	MLogRegrIRLSTransitionState<MutableArrayHandle<double> > state = args[0];

	// Get x as a vector of double
	MappedColumnVector x = args[3].getAs<MappedColumnVector>();
	
	// Get the category & numCategories as integer
	int category = args[1].getAs<int>();	
	// Number of categories after pivoting (We pivot around the first category)
	int numCategories = (args[2].getAs<int>() - 1);

	// The following check was added with MADLIB-138.
	if (!x.is_finite())
			throw std::domain_error("Design matrix is not finite.");


	if (state.numRows == 0) {
			if (x.size() > std::numeric_limits<uint16_t>::max())
					throw std::domain_error("Number of independent variables cannot be "
							"larger than 65535.");

			if (numCategories < 1)
					throw std::domain_error("Number of cateogires must be atleast 2");

			if (category > numCategories)
					throw std::domain_error("You have entered a category > numCategories"
						"Categories must be of values {0,1... numCategories-1}");

			// Init the state (requires x.size() and category.size())
			state.initialize(*this, static_cast<uint16_t>(x.size())
														, static_cast<uint16_t>(numCategories));
														
			if (!args[4].isNull()) {
					MLogRegrIRLSTransitionState<ArrayHandle<double> > previousState = args[4];
					state = previousState;
					state.reset(); 
			}
	}


	/* 	Get y as a integer: Convert to 1/-1 boolean vector
			Example: Category 4 : -1 -1 -1 1 -1 -1
			Storing it in this forms helps us get a nice closed form expression
			for the future
	*/

	// Now do the transition step
	state.numRows++;


	ColumnVector y(numCategories);

	for (int j=0; j< numCategories ; j++){
		y(j) = (j == category-1) ? 1. : -1.;
	}
	


	// state.coef[k] predict the importance of the feature k
	ColumnVector xc(numCategories);
	
	// Block wise dot products
	for (int j=0; j< numCategories ; j++){
	
		double dotProd = 0;
		for (int i = 0; i < state.widthOfX; i++){			
			dotProd += state.coef[j*state.widthOfX + i] * x[i];
		}

		xc(j) = dotProd;
	}


	/* 
		 a is a matrix of size JxJ where J is the number of categories
		 a_j1j2 = sigma(xc_j1) * sigma(-xc_j2) if j1 == j2
		 a_j1j2 = sigma(xc_j1) * sigma(xc_j2) if j1 != j2
		 Note: sigma(-x) = 1 - sigma(x)
	 
	*/
	Matrix a(numCategories,numCategories);
	
	// Loop through each row of the matrix and compute the elements
	for (int j1 = 0; j1 < numCategories ; j1++){
		for (int j2= 0; j2 < numCategories ; j2++){
			a(j1,j2)= j1==j2 
								? sigma(xc(j1))*sigma(-xc(j2))
								: sigma(xc(j2))*sigma(xc(j2));
		}
	}
	
	
	 
	//
	//              sum_j2   x_j [ y_j2 - sigma(x_j2) ]
	// z = xc_j +          ------------------------------
	//                              a_jj2
	//
	// To avoid overflows if a_i is close to 0, we do not compute z directly,
	// but instead compute a * z.
	ColumnVector az(numCategories);
	ColumnVector X_transp_Az(numCategories*state.widthOfX);
	for (int j1 = 0; j1 < numCategories ; j1++){
		double sum = 0;	    
		for (int j2 = 0 ; j2 < numCategories; j2++){
			sum += xc(j1) * a(j1,j2);
		}
			sum += sigma(-y(j1) * xc(j1)) * y(j1);
			az(j1) = sum;
	}


	for (int j1 = 0; j1 < numCategories ; j1++){	
		for (int j2 = 0; j2 < state.widthOfX ; j2++){	
			X_transp_Az(state.widthOfX * j1 + j2 ) = x(j2)*az(j1);
		}
	}
	 
	state.X_transp_Az.noalias() += X_transp_Az;
	
	Matrix X_transp_AX(numCategories*state.widthOfX,numCategories*state.widthOfX);
	
	// Duplicating the code for x*transpose(x) * a but in the multinomial case
	for (int i1 = 0; i1 < numCategories; i1++){ //the first index of a
	 	for (int i2 = 0; i2 < numCategories; i2++){ //the second index of a
			int rowOffset = state.widthOfX * i1;
			int colOffset = state.widthOfX * i2;
			/* FIXME: The two inner loops are simply moving the x* transpose(x)*a into 
				 the triangular view matrix.  Ideally, this would be done in some sort 
				 of block format, rather than writing each element individually.  
			*/
			
			for (int j1 = 0; j1 < state.widthOfX; j1++){ //The first index of x
				for (int j2 = 0; j2 < state.widthOfX; j2++){ // the second index of x
					//FIXME: replace the x(j1) * x(j2) with the multiplication done outside the nested for-loop.  
					X_transp_AX(rowOffset + j1,colOffset + j2) = (x[j1] * x[j2])*a(i1,i2); 	
				}			
			}
		}
	}

	triangularView<Lower>(state.X_transp_AX) += X_transp_AX;

	//          n
	//         --
	// l(c) = -\  ln(1 + exp(-y_i * c^T x_i))
	//         /_
	//         i=1
	state.logLikelihood -= std::log( 1. + std::exp(-trans(y)*xc) );

	return state;

}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
	 This merge step is the same as that of logistic regression.
 */
AnyType
mlogregr_irls_step_merge_states::run(AnyType &args) {
    MLogRegrIRLSTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    MLogRegrIRLSTransitionState<ArrayHandle<double> > stateRight = args[1];

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
 * @brief Perform the logistic-regression final step
 */
AnyType
mlogregr_irls_step_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    MLogRegrIRLSTransitionState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    // See MADLIB-138. At least on certain platforms and with certain versions,
    // LAPACK will run into an infinite loop if pinv() is called for non-finite
    // matrices. We extend the check also to the dependent variables.
    if (!state.X_transp_AX.is_finite() || !state.X_transp_Az.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
            "calulation. Input data is likely of poor numerical condition.");

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.X_transp_AX, EigenvaluesOnly, ComputePseudoInverse);

    // Precompute (X^T * A * X)^+
    Matrix inverse_of_X_transp_AX = decomposition.pseudoInverse();

    state.coef.noalias() = inverse_of_X_transp_AX * state.X_transp_Az;
    if(!state.coef.is_finite())
        throw NoSolutionFoundException("Over- or underflow in Newton step, "
            "while updating coefficients. Input data is likely of poor "
            "numerical condition.");

    // We use the intra-iteration field X_transp_Az for storing the diagonal
    // of X^T A X, so that we don't have to recompute it in the result function.
    // Likewise, we store the condition number.
    // FIXME: This feels a bit like a hack.
    state.X_transp_Az = inverse_of_X_transp_AX.diagonal();
    state.X_transp_AX(0,0) = decomposition.conditionNo();

    return state;
}

/**
 * @brief Return the difference in log-likelihood between two states
 */
AnyType
internal_mlogregr_irls_step_distance::run(AnyType &args) {
    MLogRegrIRLSTransitionState<ArrayHandle<double> > stateLeft = args[0];
    MLogRegrIRLSTransitionState<ArrayHandle<double> > stateRight = args[1];

    return std::abs(stateLeft.logLikelihood - stateRight.logLikelihood);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_mlogregr_irls_result::run(AnyType &args) {
    MLogRegrIRLSTransitionState<ArrayHandle<double> > state = args[0];

    return mLogstateToResult(*this, state.coef,
        state.X_transp_Az, state.logLikelihood, state.X_transp_AX(0,0));
}


/**
 * @brief Compute the diagnostic statistics
 *
 * This function wraps the common parts of computing the results for IRLS.
 */
AnyType mLogstateToResult(
    const Allocator &inAllocator,
    const HandleMap<const ColumnVector, TransparentHandle<double> > &inCoef,
    const ColumnVector &diagonal_of_inverse_of_X_transp_AX,
    double logLikelihood,
    double conditionNo) {

    MutableMappedColumnVector stdErr(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableMappedColumnVector waldZStats(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableMappedColumnVector waldPValues(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableMappedColumnVector oddsRatios(
        inAllocator.allocateArray<double>(inCoef.size()));

    for (Index i = 0; i < inCoef.size(); ++i) {
        stdErr(i) = std::sqrt(diagonal_of_inverse_of_X_transp_AX(i));
        waldZStats(i) = inCoef(i) / stdErr(i);
        waldPValues(i) = 2. * prob::cdf( prob::normal(),
            -std::abs(waldZStats(i)));
        oddsRatios(i) = std::exp( inCoef(i) );
    }

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << inCoef << logLikelihood << stdErr << waldZStats << waldPValues
        << oddsRatios << conditionNo;
    return tuple;
}

} // namespace regress

} // namespace modules

} // namespace madlib
