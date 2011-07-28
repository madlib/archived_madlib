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
using arma::vec;
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
    TransitionState(AnyType inArg)
        : mStorage(inArg.cloneIfImmutable()) {
        
        rebind();
    }

    /**
     * We define this function so that we can use TransitionState in the argument
     * list and as a return type.
     */
    inline operator AnyType() const {
        return mStorage;
    }
    
    /**
     * @brief Initialize the transition state. Only called for first row.
     * 
     * @param inAllocator Will be used to allocate the memory for the
     *     transition state. Must fill the memory block with zeros.
     */
    inline void initialize(AllocatorSPtr inAllocator,
        const uint16_t inWidthOfX) {
        
        mStorage.rebind(inAllocator, boost::extents[ arraySize(inWidthOfX) ]);
        rebind(inWidthOfX);
        widthOfX = inWidthOfX;
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
    
    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX If this value is positive, use it as the number of
     *     independent variables. This is needed during initialization, when
     *     the storage array is still all zero, but we do already know the
     *     with of the design matrix.
     */
    void rebind(uint16_t inWidthOfX = 0) {
        numRows.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        
        if (inWidthOfX == 0)
            inWidthOfX = widthOfX;
        
        y_sum.rebind(&mStorage[2]);
        y_square_sum.rebind(&mStorage[3]);
        X_transp_Y.rebind(
            TransparentHandle::create(&mStorage[4]),
            inWidthOfX);
        X_transp_X.rebind(
            TransparentHandle::create(&mStorage[4 + inWidthOfX]),
            inWidthOfX, inWidthOfX);
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
AnyType LinearRegression::transition(AbstractDBInterface &db, AnyType args) {
    AnyType::iterator arg(args);
    
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
        state.initialize(
            db.allocator(
                AbstractAllocator::kAggregate,
                AbstractAllocator::kZero
            ),
            x.n_elem);
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
AnyType LinearRegression::mergeStates(AbstractDBInterface & /* db */, AnyType args) {
    TransitionState stateLeft = args[0].cloneIfImmutable();
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
AnyType LinearRegression::final(AbstractDBInterface &db, AnyType args) {
    const TransitionState state = args[0];

    // See MADLIB-138. At least on certain platforms and with certain versions,
    // LAPACK will run into an infinite loop if pinv() is called for non-finite
    // matrices. We extend the check also to the dependent variables.
    if (!state.X_transp_X.is_finite() || !state.X_transp_Y.is_finite())
        throw std::invalid_argument("Design matrix is not finite.");
        
    // FIXME: We have essentially two calls to svd now (pinv calls svd, too).
    // This is a waste of processor cycles and energy.
    vec singularValues = svd(state.X_transp_X);
    double condition_X_transp_X = max(singularValues) / min(singularValues);

    // See:
    // Lichtblau, Daniel and Weisstein, Eric W. "Condition Number."
    // From MathWorld--A Wolfram Web Resource.
    // http://mathworld.wolfram.com/ConditionNumber.html
    if (condition_X_transp_X > 1000)
        db.out << "Matrix X^T X is ill-conditioned (condition number "
            "= " << condition_X_transp_X << "). "
            "Expect strong multicollinearity." << std::endl;
    
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
    
    // With infinite precision, the following checks are pointless. But due to
    // floating-point arithmetic, this need not hold at this point.
    // Without a formal proof convincing us of the contrary, we should
    // anticipate that numerical peculiarities might occur.
    if (tss < 0)
        tss = 0;
    if (ess < 0)
        ess = 0;
    // Since we know tss with greater accuracy than ess, we do the following
    // sanity adjustment to ess:
    if (ess > tss)
        ess = tss;

    // coefficient of determination
    // If tss == 0, then the regression perfectly fits the data, so the
    // coefficient of determination is 1.
    double r2 = (tss == 0 ? 1 : ess / tss);

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
        // In an abundance of caution, we see a tiny possibility that numerical
        // instabilities in the pinv operation can lead to negative values on
        // the main diagonal of even a SPD matrix
        if (inverse_of_X_transp_X(i,i) < 0) {
            stdErr(i) = 0;
        } else {
            stdErr(i) = std::sqrt( variance * inverse_of_X_transp_X(i,i) );
        }
        
        if (coef(i) == 0 && stdErr(i) == 0) {
            // In this special case, 0/0 should be interpreted as 0:
            // We know that 0 is the exact value for the coefficient, so
            // the t-value should be 0 (corresponding to a p-value of 1)
            tStats(i) = 0;
        } else {
            // If stdErr(i) == 0 then abs(tStats(i)) will be infinity, which is
            // what we need.
            tStats(i) = coef(i) / stdErr(i);
        }
    }
    
    // Vector of p-values: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    DoubleCol pValues(db.allocator(), state.widthOfX);
    for (int i = 0; i < state.widthOfX; i++)
        pValues(i) = 2. * (1. - studentT_cdf(
                                    state.numRows - state.widthOfX,
                                    std::fabs( tStats(i) )));
    
    // Return all coefficients, standard errors, etc. in a tuple
    AnyTypeVector tuple;
    ConcreteRecord::iterator tupleElement(tuple);
    
    *tupleElement++ = coef;
    *tupleElement++ = r2;
    *tupleElement++ = stdErr;
    *tupleElement++ = tStats;
    *tupleElement   = pValues;
    
    return tuple;
}

} // namespace regress

} // namespace modules

} // namespace madlib
