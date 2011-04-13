/* ----------------------------------------------------------------------- *//**
 *
 * @file linear.cpp
 *
 * @brief Linear-Regression functions
 *
 *//* ----------------------------------------------------------------------- */

#include <madlib/modules/regress/linear.hpp>
#include <madlib/modules/prob/student.hpp>

// Import names from Armadillo
using arma::mat;
using arma::as_scalar;

namespace madlib {

namespace modules {

// Import names from other MADlib modules
using prob::studentT_cdf;

namespace regress {

/**
 * @brief Transition state for linear-regression functions
 *
 * LinRegState encapsualtes the transition state during the linear-regression
 * aggregate functions. To the database, the state is exposed as a single
 * DOUBLE PRECISION array, to the C++ code it is a proper object containing
 * scalars, a vector, and a matrix.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 4, and all elemenets are 0.
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
          X_transp_Y(
            TransparentHandle::create(&mStorage[4]),
            widthOfX()),
          X_transp_X(
            TransparentHandle::create(&mStorage[4] + static_cast<uint32_t>(widthOfX())),
            widthOfX(), widthOfX()) { }

    /**
     * We define this function so that we can use TransitionState in the argument
     * list and as a return type.
     */
    inline operator AnyValue() {
        return mStorage;
    }
    
    /**
     * @brief Initialize the transition state. Only called for first row.
     */
    inline void initialize(AllocatorSPtr inAllocator,
        const uint16_t inWidthOfX) {
        
        mStorage.rebind(inAllocator, boost::extents[ arraySize(inWidthOfX) ]);
        numRows() = 0;
        widthOfX() = inWidthOfX;
        y_sum() = 0;
        y_square_sum() = 0;
        X_transp_Y.rebind(
            TransparentHandle::create(&mStorage[4]),
            inWidthOfX);
        X_transp_X.rebind(
            TransparentHandle::create(&mStorage[4] + inWidthOfX),
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
        
        widthOfX() = inOtherState.widthOfX();
        return *this;
    }
    
    inline double &numRows() { return mStorage[0]; }
    inline double numRows() const { return mStorage[0]; }

    inline double &widthOfX() { return mStorage[1]; }
    inline double widthOfX() const { return mStorage[1]; }

    inline double &y_sum() { return mStorage[2]; }
    inline double y_sum() const { return mStorage[2]; }
    
    inline double &y_square_sum() { return mStorage[3]; }
    inline double y_square_sum() const { return mStorage[3]; }
    
private:
    inline uint32_t arraySize(const uint16_t inWidthOfX) {
        return 4 + inWidthOfX + inWidthOfX * inWidthOfX;
    }

    Array<double> mStorage;

public:
    DoubleCol X_transp_Y;
    DoubleMat X_transp_X;
};

/**
 * @brief Compute the linear-regression coefficient as final step
 */
AnyValue LinearRegression::coefFinal(AbstractDBInterface &db, AnyValue args) {
    return final<kCoef>(db, args[0]);
}

/**
 * @brief Compute the coefficient of determination as final step
 */
AnyValue LinearRegression::RSquareFinal(AbstractDBInterface &db, AnyValue args) {
    return final<kRSquare>(db, args[0]);
}

/**
 * @brief Compute the vector of t-statistics as final step
 */
AnyValue LinearRegression::tStatsFinal(AbstractDBInterface &db, AnyValue args) {
    return final<kTStats>(db, args[0]);
}

/**
 * @brief Compute the vector of p-values as final step
 */
AnyValue LinearRegression::pValuesFinal(AbstractDBInterface &db, AnyValue args) {
    return final<kPValues>(db, args[0]);
}

/**
 * @brief Perform the linear-regression transition step
 * 
 * We update: the number of rows $n$, the partial sums \f$ \sum_{i=1}^n y_i \f$
 * and \f$ \sum_{i=1}^n y_i^2 \f$, the matrix \f$ X^T X \F$, and the vector
 * \f$ X^T \boldsymbol y \f$.
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
    
    // Now do the transition step.
    if (state.numRows() == 0)
        state.initialize(db.allocator(AbstractAllocator::kAggregate), x.n_elem);
    state.numRows()++;
    state.y_sum() += y;
    state.y_square_sum() += y * y;
    state.X_transp_Y += trans(x) * y;
    state.X_transp_X += trans(x) * x;
        
    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyValue LinearRegression::preliminary(AbstractDBInterface &db, AnyValue args) {
    TransitionState stateLeft = args[0].copyIfImmutable();
    const TransitionState stateRight = args[1];
    
    // Merge states together and return
    stateLeft += stateRight;
    return stateLeft;
}

/**
 * @brief Perform the linear-regression final step
 *
 * @internal We pass \c what as a compile-time argument.
 */
template <LinearRegression::What what>
AnyValue LinearRegression::final(AbstractDBInterface &db,
    const LinearRegression::TransitionState &state) {

    // Vector of coefficients: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    DoubleCol coef(db.allocator(), state.widthOfX());
    coef = pinv(state.X_transp_X) * state.X_transp_Y;
    if (what == kCoef)
        return coef;
    
    // explained sum of squares (regression sum of squares)
    double ess
        = as_scalar(
            trans(state.X_transp_Y) * coef
            - ((state.y_sum() * state.y_sum()) / state.numRows())
          );

    // coefficient of determination
    if (what == kRSquare) {
        // total sum of squares
        double tss
            = state.y_square_sum()
                - ((state.y_sum() * state.y_sum()) / state.numRows());
    
        return ess / tss;
    }

    // Variance is also called the mean square error
	double variance = ess / (state.numRows() - state.widthOfX());

    // Precompute (X^T * X)^{-1}
    mat inverse_of_X_transp_X = inv(state.X_transp_X);
    
    // Vector of t-statistics: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    DoubleCol tStats(db.allocator(), state.widthOfX());
    for (int i = 0; i < state.widthOfX(); i++)
        tStats(i) = coef(i) / std::sqrt( variance * inverse_of_X_transp_X(i,i) );
    if (what == kTStats)
        return tStats;
    
    // Vector of p-values: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    DoubleCol pValues(db.allocator(), state.widthOfX());
    for (int i = 0; i < state.widthOfX(); i++)
        pValues(i) = 2. * (1. - studentT_cdf(
                                    state.numRows() - state.widthOfX(),
                                    std::fabs( tStats(i) )));
    return pValues;
}

} // namespace regress

} // namespace modules

} // namespace madlib
