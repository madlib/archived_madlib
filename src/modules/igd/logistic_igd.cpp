/* ----------------------------------------------------------------------- *//**
 *
 * @file logistic.cpp
 *
 * @brief Logistic-Regression functions
 *
 * We implement the incremental gradient method.
 *
 *//* ----------------------------------------------------------------------- */

#include <modules/igd/logistic_igd.hpp>
#include <utils/Reference.hpp>

// Floating-point classification functions are in C99 and TR1, but not in the
// official C++ Standard (before C++0x). We therefore use the Boost implementation
#include <boost/math/special_functions/fpclassify.hpp>

// z values are normally distributed
#include <boost/math/distributions/normal.hpp>

// The squared z values of regression coefficients are approximately chi-squared
// distributed.
#include <boost/math/distributions/chi_squared.hpp>


// Import names from Armadillo
using arma::mat;
using arma::trans;
using arma::colvec;
using arma::as_scalar;


namespace madlib {

using utils::Reference;

namespace modules {

namespace igd {

// Local functions
AnyType stateToResult(AbstractDBInterface &db,
    const DoubleCol &inCoef,
    const double &inLogLikelihood,
    const mat &inInverse_of_X_transp_AX);

/**
 * @brief Logistic function
 */
static inline double sigma(double x) {
	if (x > 30) { return 1. / (1. + std::exp(-x)); }
	else { return std::exp(x) / (1. + std::exp(x)); }
}

/**
 * @brief Inter- and intra-iteration state for incremental gradient
 *        method for logistic regression
 *
 * TransitionState encapsualtes the transition state during the
 * logistic-regression aggregate function. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars, a vector, and a matrix.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 4, and all elemenets are 0.
 *
 * @internal Array layout (iteration refers to one aggregate-function call):
 * Inter-iteration components (updated in final function):
 * - 0: widthOfX (number of coefficients)
 * - 1: stepsize (step size of gradient steps)
 * - 2: coef (vector of coefficients)
 *
 * Intra-iteration components (updated in transition step):
 * - 2 + widthOfX: numRows (number of rows already processed in this iteration)
 * - 3 + widthOfX: X_transp_AX (X^T A X)
 * - 3 + widthOfX * widthOfX + widthOfX: logLikelihood ( ln(l(c)) )
 */
class LogisticRegressionIGD::State {
public:
    State(AnyType inArg)
        : mStorage(inArg.cloneIfImmutable()) {
        // HAYING: to list all possible inArg and understand cloneIfImmutable
        rebind();
    }
    
    /**
     * We define this function so that we can use State in the
     * argument list and as a return type.
	 *
	 * HAYING: This function makes AnyType can be assigned to State, and vice versa.
     */
    inline operator AnyType() const {
        return mStorage;
    }
    
    /**
     * @brief Initialize the iteratively-reweighted-least-squares state.
     * 
     * This function is only called for the first iteration, for the first row.
	 *
	 * HAYING: NOT TRUE, called for each iteration now, actually necessary
     */
    inline void initialize(AllocatorSPtr inAllocator,
        const uint16_t inWidthOfX) {
        
        mStorage.rebind(inAllocator, boost::extents[ arraySize(inWidthOfX) ]);
        rebind(inWidthOfX);
    }
    
    /**
     * @brief We need to support assigning the previous state
     */
    State &operator=(const State &inOtherState) {
        mStorage = inOtherState.mStorage;
        return *this;
    }
    
    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    State &operator+=(const State &inOtherState) {
        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");
        
		// HAYING: model averaging
		double totalNumRows = numRows + inOtherState.numRows;
		coef = (1. * numRows) / totalNumRows * coef
			+ (1. * inOtherState.numRows) / totalNumRows * inOtherState.coef;

        numRows += inOtherState.numRows;
        X_transp_AX += inOtherState.X_transp_AX;
        logLikelihood += inOtherState.logLikelihood;
        return *this;
    }
    
    /**
     * @brief Who should own TransparentHandles pointing to slices of mStorage?
     */
    AbstractHandle::MemoryController memoryController() const {
        AbstractHandle::MemoryController ctrl =
            mStorage.memoryHandle()->memoryController();
        
        return (ctrl == AbstractHandle::kSelf ? AbstractHandle::kLocal : ctrl);
    }

    /**
     * @brief Rebind vector to a particular position in storage array
     */
    void inline rebindToPos(DoubleCol &inVec, size_t inPos) {
        inVec.rebind(
            TransparentHandle::create(&mStorage[inPos],
                widthOfX * sizeof(double),
                memoryController()),
            widthOfX);
    }    
    
    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX If this value is positive, use it as the number of
     *     independent variables. This is needed during initialization, when
     *     the storage array is still all zero, but we do already know the
     *     with of the design matrix.
     */
    void rebind(uint16_t inWidthOfX = 0) {
        widthOfX.rebind(&mStorage[0]);
        if (inWidthOfX != 0)
            widthOfX = inWidthOfX;
        
        stepsize.rebind(&mStorage[1]);

        rebindToPos(coef, 2);
        
        numRows.rebind(&mStorage[2 + widthOfX]);
        X_transp_AX.rebind(
            TransparentHandle::create(&mStorage[3 + widthOfX],
                widthOfX * widthOfX * sizeof(double),
                memoryController()),
            widthOfX, widthOfX);
        logLikelihood.rebind(&mStorage[3 + widthOfX * widthOfX + widthOfX]);
    }
    
    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
		// HAYING: stepsize if hard-coded here now
        stepsize = .1;
        numRows = 0;
        X_transp_AX.zeros();
        logLikelihood = 0;
    }
    
private:
    static inline uint32_t arraySize(const uint16_t inWidthOfX) {
        return 4 + inWidthOfX * inWidthOfX + inWidthOfX;
    }

    Array<double> mStorage;

public:
    Reference<double, uint16_t> widthOfX;
    Reference<double> stepsize;
    DoubleCol coef;

    Reference<double, uint64_t> numRows;
	DoubleMat X_transp_AX;
    Reference<double> logLikelihood;
};

AnyType LogisticRegressionIGD::transition(AbstractDBInterface &db,
    AnyType args) {
    AnyType::iterator arg(args);
    
    // Initialize Arguments from SQL call
    State state = *arg++;
    double y = *arg++ ? 1. : -1.;
    DoubleRow_const x = *arg++;

    // See MADLIB-138. At least on certain platforms and with certain versions,
    // LAPACK will run into an infinite loop if pinv() is called for non-finite
    // matrices. We extend the check also to the dependent variables.
	// HAYING: Can I remove this if I don't need pinv()?
    if (!boost::math::isfinite(y))
        throw std::invalid_argument("Dependent variables are not finite.");
    else if (!x.is_finite())
        throw std::invalid_argument("Design matrix is not finite.");

	// HAYING: check if it is first tuple
    if (state.numRows == 0) {
        state.initialize(
            db.allocator( // HAYING: the wrapper is too heavy to understand
                AbstractAllocator::kAggregate,
                AbstractAllocator::kZero),
            x.n_elem);
		// HAYING: x.n_elem only works for dense vector, 
		//         better to be passed from the previousState 

		// HAYING: NULL is passed for the first iteration
        if (!arg->isNull()) {
			const State previousState = *arg;
			// HAYING: deep copy, I don't know whether any memory allocation
            state = previousState;
        }
		state.reset();
    }
    
    // Now do the transition step
    state.numRows++;

    // grad_i(c) = sigma(-y_i * c^T x_i) * y_i * x_i
    // Note: sigma(-x) = 1 - sigma(x)
    double xc = as_scalar( x * state.coef );
    double sig = sigma(-xc * y);
    double scale = state.stepsize * sig * y;
	state.coef += scale * trans(x);

    // Note: previous coefficients are used for Hessian and log likelihood
	if (!arg->isNull()) {
		const State previousState = *arg;
		double previous_xc = as_scalar( x * previousState.coef );
		// a_i = sigma(x_i c) sigma(-x_i c)
		double a = sigma(previous_xc) * sigma(-previous_xc);
		state.X_transp_AX += trans(x) * a * x;
			
		// We use state.sumy to store the negative log likelihood (minimizing).
		// l_i(c) = - ln(1 + exp(-y_i * c^T x_i))
		state.logLikelihood -= std::log( 1. + std::exp(-y * previous_xc) );
	}

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType LogisticRegressionIGD::mergeStates(AbstractDBInterface & /* db */,
    AnyType args) {
    
    State stateLeft = args[0].cloneIfImmutable();
    const State stateRight = args[1];
    
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
AnyType LogisticRegressionIGD::final(AbstractDBInterface & /* db */,
    AnyType args) {
    
    // Argument from SQL call
    State state = args[0].cloneIfImmutable();

    return state;
}

/**
 * @brief Return the difference in log-likelihood between two states
 */
AnyType LogisticRegressionIGD::distance(AbstractDBInterface & /* db */,
    AnyType args) {
    
    const State stateLeft = args[0];
    const State stateRight = args[1];

    return std::abs(stateLeft.logLikelihood - stateRight.logLikelihood);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType LogisticRegressionIGD::result(AbstractDBInterface &db, AnyType args) {
    const State state = args[0];

    // Compute (X^T * A * X)^+
    mat inverse_of_X_transp_AX = pinv(state.X_transp_AX);
    
    return stateToResult(db, state.coef, state.logLikelihood,
        inverse_of_X_transp_AX);
}

/**
 * @brief Compute the diagnostic statistics
 */
AnyType stateToResult(AbstractDBInterface &db,
    const DoubleCol &inCoef,
    const double &inLogLikelihood,
    const mat &inInverse_of_X_transp_AX) {
    
    DoubleCol stdErr(db.allocator(), inCoef.n_elem);
    DoubleCol waldZStats(db.allocator(), inCoef.n_elem);
    DoubleCol waldPValues(db.allocator(), inCoef.n_elem);
    DoubleCol oddsRatios(db.allocator(), inCoef.n_elem);
    for (unsigned int i = 0; i < inCoef.n_elem; i++) {
        stdErr(i) = std::sqrt(inInverse_of_X_transp_AX(i,i));
        waldZStats(i) = inCoef(i) / stdErr(i);
        waldPValues(i) = 2. * ( boost::math::cdf(boost::math::normal(),
            -std::abs( waldZStats(i) )) );
        oddsRatios(i) = std::exp( inCoef(i) );
    }

    // Return all coefficients, standard errors, etc. in a tuple
    AnyTypeVector tuple;
    ConcreteRecord::iterator tupleElement(tuple);
    
    *tupleElement++ = inCoef;
    *tupleElement++ = inLogLikelihood;
    *tupleElement++ = stdErr;
    *tupleElement++ = waldZStats;
    *tupleElement++ = waldPValues;
    *tupleElement   = oddsRatios;
    
    return tuple;
}

} // namespace incremental

} // namespace modules

} // namespace madlib
