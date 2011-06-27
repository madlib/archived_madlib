/* ----------------------------------------------------------------------- *//**
 *
 * @file linear.cpp
 *
 * @brief Linear-regression functions
 *
 *//* ----------------------------------------------------------------------- */

#include <modules/regress/linear.hpp>
#include <modules/prob/student.hpp>
#include <utils/Reference.hpp>

// Floating-point classification functions are in C99 and TR1, but not in the
// official C++ Standard (before C++0x). We therefore use the Boost implementation
#include <boost/math/special_functions/fpclassify.hpp>


// Import names from Armadillo
using arma::mat;
using arma::as_scalar;

namespace madlib {

using utils::Reference;

namespace modules {

// Import names from other MADlib modules
using prob::studentT_cdf;

namespace regress {

/**
 * @brief Transition state for linear-regression functions
 *
 * TransitionState encapsualtes the transition state during the
 * linear-regression aggregate functions. To the database, the state is exposed
 * as a single DOUBLE PRECISION array, to the C++ code it is a proper object
 * containing scalars, a vector, and a matrix.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elemenets are 0.
 */
class LinearRegression::TransitionState {
public:
    /**
     * @internal Member initalization occurs in the order of declaration in the
     *      class (see ISO/IEC 14882:2003, Section 12.6.2). The order in the
     *      init list is irrelevant. It is important that mStorage gets
     *      initialized before the other members!
     */
    TransitionState(AnyValue inArg)
        : mStorage(inArg.copyIfImmutable()),
          numRows(&mStorage[0]),
          widthOfX(&mStorage[1]),
          y_sum(&mStorage[2]),
          y_square_sum(&mStorage[3]),
          X_transp_Y(
            TransparentHandle::create(&mStorage[4]),
            widthOfX),
          X_transp_X(
            TransparentHandle::create(&mStorage[4 + widthOfX]),
            widthOfX, widthOfX) { }

    /**
     * We define this function so that we can use TransitionState in the argument
     * list and as a return type.
     */
    inline operator AnyValue() const {
        return mStorage;
    }
    
    /**
     * @brief Initialize the transition state. Only called for first row.
     */
    inline void initialize(AllocatorSPtr inAllocator,
        const uint16_t inWidthOfX) {
        
        mStorage.rebind(inAllocator, boost::extents[ arraySize(inWidthOfX) ]);
        numRows.rebind(&mStorage[0]) = 0;
        widthOfX.rebind(&mStorage[1]) = inWidthOfX;
        y_sum.rebind(&mStorage[2]) = 0;
        y_square_sum.rebind(&mStorage[3]) = 0;
        X_transp_Y.rebind(
            TransparentHandle::create(&mStorage[4]),
            inWidthOfX);
        X_transp_X.rebind(
            TransparentHandle::create(&mStorage[4 + inWidthOfX]),
            inWidthOfX, inWidthOfX);
    }
    
    /**
     * @brief Merge with another TransitionState object
     */
    TransitionState &operator+=(const TransitionState &inOtherState) {
        if (mStorage.size() != inOtherState.mStorage.size())
            throw std::logic_error("Internal error: Incompatible transition states");
            
        for (uint32_t i = 0; i < mStorage.size(); i++)
            mStorage[i] += inOtherState.mStorage[i];
        
        widthOfX = inOtherState.widthOfX;
        return *this;
    }
        
private:
    static inline uint32_t arraySize(const uint16_t inWidthOfX) {
        return 4 + inWidthOfX + inWidthOfX * inWidthOfX;
    }

    Array<double> mStorage;

public:
    Reference<double, uint64_t> numRows;
    Reference<double, uint16_t> widthOfX;
    Reference<double> y_sum;
    Reference<double> y_square_sum;
    DoubleCol X_transp_Y;
    DoubleMat X_transp_X;
};

/**
 * @brief Perform the linear-regression transition step
 * 
 * We update: the number of rows \f$ n \f$, the partial sums
 * \f$ \sum_{i=1}^n y_i \f$ and \f$ \sum_{i=1}^n y_i^2 \f$, the matrix
 * \f$ X^T X \f$, and the vector \f$ X^T \boldsymbol y \f$.
 */
AnyValue LinearRegression::transition(AbstractDBInterface &db, AnyValue args) {
    AnyValue::iterator arg(args);
    
    // Arguments from SQL call. Immutable values passed by reference should be
    // instantiated from the respective <tt>_const</tt> class. Otherwise, the
    // abstraction layer will perform a deep copy (i.e., waste unnecessary
    // processor cycles).
    TransitionState state = *arg++;
    double y = *arg++;
    DoubleRow_const x = *arg++;
    
    // See MADLIB-138. At least on certain platforms and with certain versions,
    // LAPACK will run into an infinite loop if pinv() is called for non-finite
    // matrices. We extend the check also to the dependent variables.
    if (!boost::math::isfinite(y))
        throw std::invalid_argument("Dependent variables are not finite.");
    else if (!x.is_finite())
        throw std::invalid_argument("Design matrix is not finite.");
    
    // Now do the transition step.
    if (state.numRows == 0)
        state.initialize(db.allocator(AbstractAllocator::kAggregate), x.n_elem);
    state.numRows++;
    state.y_sum += y;
    state.y_square_sum += y * y;
    state.X_transp_Y += trans(x) * y;
    state.X_transp_X += trans(x) * x;
        
    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyValue LinearRegression::mergeStates(AbstractDBInterface &db, AnyValue args) {
    TransitionState stateLeft = args[0].copyIfImmutable();
    const TransitionState stateRight = args[1];
    
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
 * @brief Perform the linear-regression final step
 */
AnyValue LinearRegression::final(AbstractDBInterface &db, AnyValue args) {
    const TransitionState state = args[0];

    // See MADLIB-138. At least on certain platforms and with certain versions,
    // LAPACK will run into an infinite loop if pinv() is called for non-finite
    // matrices. We extend the check also to the dependent variables.
    if (!state.X_transp_X.is_finite() || !state.X_transp_Y.is_finite())
        throw std::invalid_argument("Design matrix is not finite.");

    // Precompute (X^T * X)^+
    mat inverse_of_X_transp_X = pinv(state.X_transp_X);

    // Vector of coefficients: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    DoubleCol coef(db.allocator(), state.widthOfX);
    coef = inverse_of_X_transp_X * state.X_transp_Y;
    
    // explained sum of squares (regression sum of squares)
    double ess
        = as_scalar(
            trans(state.X_transp_Y) * coef
            - ((state.y_sum * state.y_sum) / state.numRows)
          );

    // total sum of squares
    double tss
        = state.y_square_sum
            - ((state.y_sum * state.y_sum) / state.numRows);

    // coefficient of determination
    double r2 = ess / tss;

    // In the case of linear regression:
    // residual sum of squares (rss) = total sum of squares (tss) - explained
    // sum of squares (ess)
    // Proof: http://en.wikipedia.org/wiki/Sum_of_squares
    double rss = tss - ess;

    // Variance is also called the mean square error
	double variance = rss / (state.numRows - state.widthOfX);
    
    // Vector of standard errors and t-statistics: For efficiency reasons, we
    // want to return these by reference, so we need to bind to db memory
    DoubleCol stdErr(db.allocator(), state.widthOfX);
    DoubleCol tStats(db.allocator(), state.widthOfX);
    for (int i = 0; i < state.widthOfX; i++) {
        stdErr(i) = std::sqrt( variance * inverse_of_X_transp_X(i,i) );
        tStats(i) = coef(i) / stdErr(i);
    }
    
    // Vector of p-values: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    DoubleCol pValues(db.allocator(), state.widthOfX);
    for (int i = 0; i < state.widthOfX; i++)
        pValues(i) = 2. * (1. - studentT_cdf(
                                    state.numRows - state.widthOfX,
                                    std::fabs( tStats(i) )));
    
    // Return all coefficients, standard errors, etc. in a tuple
    AnyValueVector tuple;
    ConcreteRecord::iterator tupleElement(tuple);
    
    tupleElement++ = coef;
    tupleElement++ = r2;
    tupleElement++ = stdErr;
    tupleElement++ = tStats;
    tupleElement++ = pValues;
    
    return tuple;
}

} // namespace regress

} // namespace modules

} // namespace madlib
