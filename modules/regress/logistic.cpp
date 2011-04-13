/* ----------------------------------------------------------------------- *//**
 *
 * @file logistic.cpp
 *
 * @brief Logistic-Regression functions
 *
 *//* ----------------------------------------------------------------------- */

#include <madlib/modules/regress/logistic.hpp>

// Import names from Armadillo
using arma::trans;
using arma::colvec;
using arma::as_scalar;

namespace madlib {

namespace modules {

namespace regress {

/**
 * @brief Inter- and intra-iteration state for conjugate-gradient method for
 *        logistic regression
 *
 * TransitionState encapsualtes the transition state during the
 * logistic-regression aggregate function. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars, a vector, and a matrix.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 6, and all elemenets are 0.
 *
 * @internal Array layout (iteration refers to one aggregate-function call):
 * Inter-iteration components (updated in final function):
 * - 0: iteration (current iteration)
 * - 1: widthOfX (numer of coefficients)
 * - 2: coef (vector of coefficients)
 * - 2 + widthOfX: dir (direction)
 * - 2 + 2 * widthOfX: grad (gradient)
 * - 2 + 3 * widthOfX: beta (scale factor)
 *
 * Intra-iteration components (updated in transition step):
 * - 3 + 3 * widthOfX: numRows (number of rows already processed in this iteration)
 * - 4 + 3 * widthOfX: gradNew (intermediate value for gradient)
 * - 4 + 4 * widthOfX: dTHd (intermediate value for d^T * H * d)
 * - 5 + 4 * widthOfX: logLikelihood ( ln(l(c)) )
 */
class LogisticRegressionCG::State {
public:
    State(AnyValue inArg)
        : mStorage(inArg.copyIfImmutable()),
          coef(
            TransparentHandle::create(&mStorage[2]),
            widthOfX()),
          dir(
            TransparentHandle::create(&mStorage[2] + static_cast<uint32_t>(widthOfX())),
            widthOfX()),
          grad(
            TransparentHandle::create(&mStorage[2] + 2 * static_cast<uint32_t>(widthOfX())),
            widthOfX()),
          gradNew(
            TransparentHandle::create(&mStorage[4] + 3 * static_cast<uint32_t>(widthOfX())),
            widthOfX())
        { }
    
    /**
     * We define this function so that we can use State in the
     * argument list and as a return type.
     */
    inline operator AnyValue() {
        return mStorage;
    }
    
    /**
     * @brief Initialize the conjugate-gradient state.
     * 
     * This function is only called for the first iteration, for the first row.
     */
    inline void initialize(AllocatorSPtr inAllocator,
        const uint16_t inWidthOfX) {
        
        mStorage.rebind(inAllocator, boost::extents[ arraySize(inWidthOfX) ]);
        iteration() = 0;
        widthOfX() = inWidthOfX;
        coef.rebind(
            TransparentHandle::create(&mStorage[2]),
            widthOfX()).zeros();
        dir.rebind(
            TransparentHandle::create(&mStorage[2] + static_cast<uint32_t>(widthOfX())),
            widthOfX()).zeros();
        grad.rebind(
            TransparentHandle::create(&mStorage[2] + 2 * static_cast<uint32_t>(widthOfX())),
            widthOfX()).zeros();
        beta() = 0;

        gradNew.rebind(
            TransparentHandle::create(&mStorage[4] + 3 * static_cast<uint32_t>(widthOfX())),
            widthOfX());
        
        reset();
    }
    
    /**
     * @brief We need to support assigning the previous state
     */
    State &operator=(const State &inOtherState) {
        mStorage = inOtherState.mStorage;
        return *this;
    }
    
    /**
     * @brief Merge with another State object by copying the intra-iteration fields
     */
    State &operator+=(const State &inOtherState) {
        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX() != inOtherState.widthOfX())
            throw std::logic_error("Internal error: Incompatible transition states");
        
        numRows() += inOtherState.numRows();
        gradNew += inOtherState.gradNew;
        dTHd() += inOtherState.dTHd();
        logLikelihood() += inOtherState.logLikelihood();
        return *this;
    }
    
    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows() = 0;
        dTHd() = 0;
        gradNew.zeros();
        logLikelihood() = 0;
    }

    inline double &iteration() { return mStorage[0]; }
    inline double iteration() const { return mStorage[0]; }
    
    inline double &widthOfX() { return mStorage[1]; }
    inline double widthOfX() const { return mStorage[1]; }
    
    inline double &beta() { return mStorage[ 2 + 3 * widthOfX() ]; }
    inline double beta() const { return mStorage[ 2 + 3 * widthOfX() ]; }
    
    inline double &numRows() { return mStorage[ 3 + 3 * widthOfX() ]; }
    inline double numRows() const { return mStorage[ 3 + 3 * widthOfX() ]; }
    
    inline double &dTHd() { return mStorage[ 4 + 4 * widthOfX() ]; }
    inline double dTHd() const { return mStorage[ 4 + 4 * widthOfX() ]; }
    
    inline double &logLikelihood() { return mStorage[ 5 + 4 * widthOfX() ]; }
    inline double logLikelihood() const { return mStorage[ 5 + 4 * widthOfX() ]; }
    
private:
    static inline uint32_t arraySize(const uint16_t inWidthOfX) {
        return 6 + 4 * inWidthOfX;
    }

    Array<double> mStorage;

public:
    DoubleCol coef;
    DoubleCol dir;
    DoubleCol grad;
    
    DoubleCol gradNew;
};

/**
 * @brief Logistic function
 */
static double sigma(double x) {
	return 1 / (1 + std::exp(-x));
}

/**
 * @brief Perform the logistic-regression transition step
 */
AnyValue LogisticRegressionCG::transition(AbstractDBInterface &db, AnyValue args) {
    AnyValue::iterator arg(args);
    
    // Initialize Arguments from SQL call
    State state = *arg++;
    int y = *arg++ ? 1 : -1;
    DoubleRow_const x = *arg++;
    if (state.numRows() == 0) {
        state.initialize(db.allocator(AbstractAllocator::kAggregate), x.n_elem);
        if (!arg->isNull()) {
            const State previousState = *arg;
            
            state = previousState;
            state.reset();
        }
    }
    
    // Now do the transition step
    state.numRows()++;
	
    double cTx = as_scalar( trans(state.coef) * x );
	double dTx = as_scalar( trans(state.dir) * x );
    
    if (static_cast<uint32_t>(state.iteration()) % 2 == 0)
        state.gradNew += sigma(y * cTx) * y * x;
    else
        state.dTHd() += sigma(cTx) * (1 - sigma(cTx)) * dTx * dTx;
    
    //          n
    //         --
    // l(c) = -\  ln(1 + exp(-y_i * c^T x_i))
    //         /_
    //         i=1
    state.logLikelihood() -= std::log( 1. + std::exp(-y * cTx) );
    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyValue LogisticRegressionCG::preliminary(AbstractDBInterface &db, AnyValue args) {
    State stateLeft = args[0].copyIfImmutable();
    const State stateRight = args[1];
    
    // Merge states together and return
    stateLeft += stateRight;
    return stateLeft;
}

/**
 * @brief Perform the logistic-regression final step
 */
AnyValue LogisticRegressionCG::coefFinal(AbstractDBInterface &db, AnyValue args) {
    // Argument from SQL call
    State state = args[0].copyIfImmutable();
    
    // Note: k = state.iteration() / 2
    if (state.iteration() == 0) {
		// Iteration computes the gradient
	
		state.dir = state.gradNew;
		state.grad = state.gradNew;
	} else if (static_cast<uint32_t>(state.iteration()) % 2 == 0) {
		// Even iterations compute the gradient (during the accumulation phase)
		// and the new direction (during the final phase).  Note that
		// state.gradNew != state.grad starting from iteration 2
		
		//            g_k^T (g_k - g_{k-1})
		// beta_k = -------------------------
		//          d_{k-1}^T (g_k - g_{k-1})
        colvec gradMinusGradOld = state.gradNew - state.grad;
        state.beta()
            = as_scalar(
                (trans(state.gradNew) * gradMinusGradOld)
            /   (trans(state.dir) * gradMinusGradOld) );
        
        // d_k = g_k - beta_k * d_{k-1}
        state.dir = state.gradNew - state.beta() * state.dir;
		state.grad = state.gradNew;
	} else {
		// Odd iteration compute -d^T H d (during the accumulation phase) and
		// and the new coefficients (during the final phase).

		//              g_k^T d_k
		// alpha_k = - -----------
		//             d_k^T H d_k
        //
		// c_k = c_{k-1} - alpha_k * d_k

		state.coef -= ( dot(state.grad, state.dir) / state.dTHd() ) * state.dir;
	}
    state.iteration()++;
    return state;
}

} // namespace regress

} // namespace modules

} // namespace madlib
