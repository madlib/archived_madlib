/* ----------------------------------------------------------------------- *//**
 *
 * @file linear.cpp
 *
 * @brief Linear-Regression functions
 *
 *//* ----------------------------------------------------------------------- */

#include <madlib/modules/regress/linear.hpp>

namespace madlib {

namespace modules {

namespace regress {

/**
 * @brief Transition State for linear-regression functions
 *
 * LinRegState encapsualtes the transition state during the linear-regression
 * aggregate functions. To the database, the state is exposed as a single
 * DOUBLE PRECISION array, to the C++ code it is a proper object containing
 * scalars, a vector, and a matrix.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 4, and all elemenets are 0.
 */
class LinRegState {
public:
    // Member initalization occurs in the order of declaration in the class (see
    // ISO/IEC 14882:2003, Section 12.6.2). The order in the init list is
    // irrelevant. It is important that mStorage gets initialized before the
    // other members!
    LinRegState(AnyValue inArg)
        : mStorage(inArg),
          X_transp_Y(
            TransparentHandle::create(&mStorage[4]),
            widthOfX()),
          X_transp_X(
            TransparentHandle::create(&mStorage[4] + static_cast<uint32_t>(widthOfX())),
            widthOfX(), widthOfX()) { }

    // We define this function so that we can use LinRegState in the argument
    // list and as a return type.
    inline operator AnyValue() {
        return mStorage;
    }
    
    // This function sets up the initial state.
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
    
    inline double &numRows() { return mStorage[0]; }

    inline double &widthOfX() { return mStorage[1]; }
    inline double widthOfX() const { return mStorage[1]; }

    inline double &y_sum() { return mStorage[2]; }
    inline double &y_square_sum() { return mStorage[3]; }
    
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
 * @brief Perform the linear-regression transition step
 * 
 * We update: the number of rows $n$, the partial sums \f$ \sum_{i=1}^n y_i \f$
 * and \f$ \sum_{i=1}^n y_i^2 \f$, the matrix \f$ X^T X \F$, and the vector
 * \f$ X^T \boldsymbol y \f$.
 */
AnyValue linearRegression::transition(AbstractDBInterface &db, AnyValue args) {
    AnyValue::iterator arg(args);
    
    // Arguments from SQL call. Values passed by reference should be declared
    // constant if the database prohibits modification. Otherwise, the
    // abstraction will perform a deep copy (i.e., waste unnecessary processor
    // cycles). It is generally a good idea to explicitly declare all function
    // arguments constant, except those that we deliberatly want to modify
    // in-place.
    LinRegState state = *arg++;
    const double y = *arg++;
    const DoubleRow x = *arg++;
    
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
 * @brief Perform the linear-regression final step
 *
 * Given \f$ X^T X \f$ and \f$ X^T y \f$ from the transition state, we compute
 * the regression coefficients via the formula:
 * \f[
 *    \boldsymbol c = (X^T X)^+ X^T \boldsymbol y
 * \f]
 * where \f$ X^+ \f$ dentoes the Moore–Penrose pseudo-inverse of $X$.
 */
AnyValue linearRegression::final(AbstractDBInterface &db, AnyValue arg) {
    // Argument from SQL call. We won't modify anything, so it's good practice
    // to declare it as constant.
    const LinRegState state = arg[0];

    // Vector of coefficients: For efficiency reasons, we want to return this
    // by reference, so we need to bind to db memory
    DoubleCol coef(db.allocator(), state.widthOfX());
    
    // Do the calculation and return the result
    coef = pinv(state.X_transp_X) * state.X_transp_Y;
    return coef;
}

} // namespace regress

} // namespace modules

} // namespace madlib
