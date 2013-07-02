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
#include <boost/math/distributions.hpp>
#include <modules/prob/student.hpp>
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
AnyType mLogstateToResult(
    const Allocator &inAllocator,
    int ref_category, 
    const HandleMap<const ColumnVector, TransparentHandle<double> >& inCoef,
    const ColumnVector &diagonal_of_heissian,
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
    inline void initialize(
        const Allocator &inAllocator, 
        uint16_t inWidthOfX, 
        uint16_t inNumCategories, uint16_t inRefCategory) {

        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX, inNumCategories));
        rebind(inWidthOfX, inNumCategories);
        widthOfX = inWidthOfX;
        numCategories = inNumCategories;
        ref_category = inRefCategory;
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle> MLogRegrIRLSTransitionState &operator=(
        const MLogRegrIRLSTransitionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle> MLogRegrIRLSTransitionState &operator+=(
        const MLogRegrIRLSTransitionState<OtherHandle> &inOtherState) {
        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");

        numRows += inOtherState.numRows;
        gradient += inOtherState.gradient;
        X_transp_AX += inOtherState.X_transp_AX;
        logLikelihood += inOtherState.logLikelihood;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        gradient.fill(0);
        X_transp_AX.fill(0);
        logLikelihood = 0;
    }

private:
    static inline uint32_t arraySize(const uint16_t inWidthOfX,
        const uint16_t inNumCategories) {
        return 5 + inWidthOfX * inWidthOfX * inNumCategories * inNumCategories
                                 + 2 * inWidthOfX * inNumCategories;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX             The number of independent variables.
     * @param inNumCategories The number of categories of the dependant var


     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: widthOfX (number of independant variables)
     * - 1: numCategories (number of categories)
     * - 2: coef (vector of coefficients)
     *
     * Intra-iteration components (updated in transition step):
     * - 2 + widthOfX*numCategories: numRows (number of rows already processed in this iteration)
     * - 3 + widthOfX*numCategories: gradient (X^T A z)
     * - 3 + 2 * widthOfX * inNumCategories: X_transp_AX (X^T A X)
     * - 3 + widthOfX^2*numCategories^2
                         + 2 * widthOfX*numCategories: logLikelihood ( ln(l(c)) )
     * - 4 + widthOfX^2*numCategories^2
                         + 2 * widthOfX*numCategories: ref_category
     */
    void rebind(uint16_t inWidthOfX = 0, uint16_t inNumCategories = 0) {
        widthOfX.rebind(&mStorage[0]);
        numCategories.rebind(&mStorage[1]);
        coef.rebind(&mStorage[2], inWidthOfX*inNumCategories);

        numRows.rebind(&mStorage[2 + inWidthOfX*inNumCategories]);

        gradient.rebind(&mStorage[3 + inWidthOfX*inNumCategories],inWidthOfX*inNumCategories);
        X_transp_AX.rebind(&mStorage[3 + 2 * inWidthOfX*inNumCategories],
            inNumCategories*inWidthOfX, inWidthOfX*inNumCategories);
        logLikelihood.rebind(&mStorage[3 +
             inNumCategories*inNumCategories*inWidthOfX*inWidthOfX
             + 2 * inWidthOfX*inNumCategories]);
        ref_category.rebind(&mStorage[4 +
             inNumCategories*inNumCategories*inWidthOfX*inWidthOfX
             + 2 * inWidthOfX*inNumCategories]);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToUInt16 numCategories;

    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap gradient;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap X_transp_AX;
    typename HandleTraits<Handle>::ReferenceToDouble logLikelihood;
    typename HandleTraits<Handle>::ReferenceToUInt16 ref_category;
};


/**
 * @brief Inter- and intra-iteration state for robust variance calculations
 *
 * TransitionState encapsualtes the transition state during the
 * logistic-regression robust variance calculation. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars, a vector, and a matrix.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 4, and all elemenets are 0.
 */
template <class Handle>
class MLogRegrRobustTransitionState {

    // By §14.5.3/9: "Friend declarations shall not declare partial
    // specializations." We do access protected members in operator+=().
    template <class OtherHandle>
    friend class MLogRegrRobustTransitionState;

public:
    MLogRegrRobustTransitionState(const AnyType &inArray)
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
    inline void initialize(const Allocator &inAllocator, 
        uint16_t inWidthOfX, uint16_t inNumCategories, uint16_t inRefCategory) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX, inNumCategories));
        rebind(inWidthOfX, inNumCategories);
        widthOfX = inWidthOfX;
        numCategories = inNumCategories;
        ref_category = inRefCategory;
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle> MLogRegrRobustTransitionState &operator=(
        const MLogRegrRobustTransitionState<OtherHandle> &inOtherState) {
        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle> MLogRegrRobustTransitionState &operator+=(
        const MLogRegrRobustTransitionState<OtherHandle> &inOtherState) {
        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");

        numRows += inOtherState.numRows;
        X_transp_AX += inOtherState.X_transp_AX;
        meat += inOtherState.meat;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        meat.fill(0);
        X_transp_AX.fill(0);
    }

private:
    static inline uint32_t arraySize(const uint16_t inWidthOfX,
        const uint16_t inNumCategories) {
        return 4 + 2*inWidthOfX * inWidthOfX * inNumCategories * inNumCategories
                                 + inWidthOfX * inNumCategories;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX             The number of independent variables.
     * @param inNumCategories The number of categories of the dependant var


     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: widthOfX (number of independant variables)
     * - 1: numCategories (number of categories)
     * - 2: ref_category
     * - 3: coef (vector of coefficients)
     *
     * Intra-iteration components (updated in transition step):
     * - 3 + widthOfX*numCategories: numRows (number of rows already processed in this iteration)
     * - 4 + widthOfX * inNumCategories: X_transp_AX (X^T A X).  
     * - 4 + widthOfX^2*numCategories^2: meat  (The meat matrix)
     */
    void rebind(uint16_t inWidthOfX = 0, uint16_t inNumCategories = 0) {
        widthOfX.rebind(&mStorage[0]);
        numCategories.rebind(&mStorage[1]);
        ref_category.rebind(&mStorage[2]);
        coef.rebind(&mStorage[3], inWidthOfX * inNumCategories);
        numRows.rebind(&mStorage[3 + inWidthOfX * inNumCategories]);
        X_transp_AX.rebind(&mStorage[4 + inWidthOfX * inNumCategories],
            inNumCategories * inWidthOfX, inWidthOfX * inNumCategories);
        meat.rebind(&mStorage[4 +
             inNumCategories * inNumCategories * inWidthOfX * inWidthOfX
             + inWidthOfX * inNumCategories], inWidthOfX * inNumCategories, inWidthOfX * inNumCategories);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToUInt16 numCategories;

    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap X_transp_AX;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap meat;
    typename HandleTraits<Handle>::ReferenceToUInt16 ref_category;
};



/**
 * @brief IRLS Transition
 * @param args
 *
 * Arguments (Matched with PSQL wrapped)
 * - 0: Current State
 * - 1: y value (Integer)
 * - 2: numCategories (Integer)
 * - 3: ref_category (Integer)
 * - 4: X value (Column Vector)
 * - 5: Previous State

 */
AnyType
__mlogregr_irls_step_transition::run(AnyType &args) {
    MLogRegrIRLSTransitionState<MutableArrayHandle<double> > state = args[0];

    // Get x as a vector of double
    MappedColumnVector x = args[4].getAs<MappedColumnVector>();

    // Get the category & numCategories as integer
    int32_t category = args[1].getAs<int32_t>();
    // Number of categories after pivoting (We pivot around the first category)
    int32_t numCategories = (args[2].getAs<int32_t>() - 1);
    int32_t ref_category = args[3].getAs<int32_t>();

    // The following check was added with MADLIB-138.
    if (!x.is_finite())
            throw std::domain_error("Design matrix is not finite.");

    if (state.numRows == 0) {
        if (x.size() > std::numeric_limits<uint16_t>::max())
                throw std::domain_error("Number of independent variables cannot be "
                        "larger than 65535.");

        if (numCategories < 1)
                throw std::domain_error("Number of cateogires must be at least 2");

        // Init the state (requires x.size() and category.size())
        state.initialize(*this, 
            static_cast<uint16_t>(x.size()) ,
            static_cast<uint16_t>(numCategories),
            static_cast<uint16_t>(ref_category));

        if (!args[5].isNull()) {
                MLogRegrIRLSTransitionState<ArrayHandle<double> > 
                    previousState = args[5];
                state = previousState;
                state.reset();
        }
    }

    /*
     * This check should be done for each iteration. Only checking the first
     * run is not enough.
     */
    if (category > numCategories || category < 0)
        throw std::domain_error("Invalid category. Categories must be integer "
            "values between 0 and (number of categories - 1).");

    if (ref_category > numCategories || ref_category < 0)
        throw std::domain_error("Invalid reference category. Reference category must be integer "
            "value between 0 and (number of categories - 1).");

    // Now do the transition step
    state.numRows++;
    /*     Get y: Convert to 1/0 boolean vector
            Example: Category 4 : 0 0 0 1 0 0
            Storing it in this forms helps us get a nice closed form expression
    */

    //To pivot around the specified reference category
    ColumnVector y(numCategories);
    y.fill(0);
    if (category > ref_category) {
        y(category - 1) = 1;
    } else if (category < ref_category) {
        y(category) = 1;
    }

    /*
    Compute the parameter vector (the 'pi' vector in the documentation)
    for the data point being processed.
    Casting the coefficients into a matrix makes the calculation simple.
    */
    Matrix coef = state.coef;
    coef.resize(numCategories, state.widthOfX);

    //Store the intermediate calculations because we'll reuse them in the LLH
    ColumnVector t1 = x; //t1 is vector of size state.widthOfX
    t1 = coef*x;
    /* Note: The above 2 lines could have been written as:
        ColumnVector t1 = -coef*x;

        but this creates warnings. These warnings are somehow related to the factor
        that x is of an immutable type. The following alternative could resolve
        the warnings (although not necessary the best alternative):
    */

    ColumnVector t2 = t1.array().exp();
    double t3 = 1 + t2.sum();
    ColumnVector pi = t2/t3;

    //The gradient matrix has numCategories rows and widthOfX columns
    Matrix grad = -y*x.transpose() + pi*x.transpose();
    //We cast the gradient into a vector to make the Newton step calculations much easier.
    grad.resize(numCategories*state.widthOfX,1);

    /*
         a is a matrix of size JxJ where J is the number of categories
         a_j1j2 = -pi(j1)*(1-pi(j2))if j1 == j2
         a_j1j2 =  pi(j1)*pi(j2) if j1 != j2
    */
    Matrix a(numCategories,numCategories);
    // Compute the 'a' matrix.
    Matrix piDiag = pi.asDiagonal();
    a = pi * pi.transpose() - piDiag;
    state.gradient.noalias() += grad;

    //Start the Hessian calculations
    Matrix X_transp_AX(numCategories * state.widthOfX, numCategories * state.widthOfX);

    /*
        Again: The following 3 lines could have been written as
        Matrix XXTrans = x * x.transpose();
        but it creates warnings related to the type of x. Here is an easy fix
    */
    Matrix cv_x = x;
    Matrix XXTrans = trans(cv_x);
    XXTrans = cv_x * XXTrans;

    //Eigen doesn't supported outer-products for matrices, so we have to do our own.
    //This operation is also known as a tensor-product.
    for (int i1 = 0; i1 < state.widthOfX; i1++){
         for (int i2 = 0; i2 <state.widthOfX; i2++){
            int rowOffset = numCategories * i1;
            int colOffset = numCategories * i2;

            X_transp_AX.block(rowOffset, colOffset, numCategories,  numCategories) = XXTrans(i1,i2)*a;
        }
    }

    triangularView<Lower>(state.X_transp_AX) += X_transp_AX;

    state.logLikelihood += y.transpose()*t1 - log(t3);

    return state;

}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
     This merge step is the same as that of logistic regression.
 */
AnyType
__mlogregr_irls_step_merge_states::run(AnyType &args) {
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
__mlogregr_irls_step_final::run(AnyType &args) {
    // We request a mutable object. 
    // Depending on the backend, this might perform a deep copy.
    MLogRegrIRLSTransitionState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    // See MADLIB-138. At least on certain platforms and with certain versions,
    // LAPACK will run into an infinite loop if pinv() is called for non-finite
    // matrices. We extend the check also to the dependent variables.
    if (!state.X_transp_AX.is_finite() || !state.gradient.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
            "calulation. Input data is likely of poor numerical condition.");

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        -1 * state.X_transp_AX, EigenvaluesOnly, ComputePseudoInverse);

    // Precompute (X^T * A * X)^-1
    Matrix heissianInver = -1 * decomposition.pseudoInverse();

    state.coef.noalias() += heissianInver * state.gradient;

    if(!state.coef.is_finite())
        throw NoSolutionFoundException("Over- or underflow in Newton step, "
            "while updating coefficients. Input data is likely of poor "
            "numerical condition.");

    // We use the intra-iteration field gradient for storing the diagonal
    // of X^T A X, so that we don't have to recompute it in the result function.
    // Likewise, we store the condition number.
    // FIXME: This feels a bit like a hack.
    state.gradient = -1 * heissianInver.diagonal();
    state.X_transp_AX(0,0) = decomposition.conditionNo();

    return state;
}

AnyType
mlogregr_robust_step_transition::run(AnyType &args) {
    using std::endl;

	MLogRegrRobustTransitionState<MutableArrayHandle<double> > state = args[0];
    // Get x as a vector of double
    MappedColumnVector x = args[4].getAs<MappedColumnVector>();
    // Get the category & numCategories as integer
    int16_t category = args[1].getAs<int>();
    // Number of categories after pivoting (We pivot around the first category)
    int16_t numCategories = (args[2].getAs<int>() - 1);
    int32_t ref_category = args[3].getAs<int32_t>();
	MappedColumnVector coefVec = args[5].getAs<MappedColumnVector>();

    // The following check was added with MADLIB-138.
    if (!x.is_finite())
            throw std::domain_error("Design matrix is not finite.");

    if (state.numRows == 0) {
        if (x.size() > std::numeric_limits<uint16_t>::max())
                throw std::domain_error("Number of independent variables cannot be "
                        "larger than 65535.");
        if (numCategories < 1)
                throw std::domain_error("Number of cateogires must be at least 2");
        if (category > numCategories)
                throw std::domain_error("You have entered a category > numCategories"
                    "Categories must be of values {0,1... numCategories-1}");

        // Init the state (requires x.size() and category.size())
        state.initialize(*this, 
            static_cast<uint16_t>(x.size()) ,
            static_cast<uint16_t>(numCategories),
            static_cast<uint16_t>(ref_category));
                                                    
        state.coef = coefVec;
    }

    // Now do the transition step
    state.numRows++;
    /*     
        Get y: Convert to 1/0 boolean vector
        Example: Category 4 : 0 0 0 1 0 0
        Storing it in this forms helps us get a nice closed form expression
    */

    //To pivot around the specified reference category
    ColumnVector y(numCategories);
    y.fill(0);
    if (category > ref_category) {
        y(category - 1) = 1;
    } else if (category < ref_category) {
        y(category) = 1;
    }

    /*
    Compute the parameter vector (the 'pi' vector in the documentation)
    for the data point being processed.
    Casting the coefficients into a matrix makes the calculation simple.
    */
    
    Matrix coef = state.coef;
    coef.resize(numCategories, state.widthOfX);

    //Store the intermediate calculations because we'll reuse them in the LLH
    ColumnVector t1 = x; //t1 is vector of size state.widthOfX
    t1 = coef*x;
    /* 
        Note: The above 2 lines could have been written as:
        ColumnVector t1 = -coef*x;

        but this creates warnings. These warnings are somehow related to the factor
        that x is an immutable type. 
    */

    ColumnVector t2 = t1.array().exp();
    double t3 = 1 + t2.sum();
    ColumnVector pi = t2/t3;

	//The gradient matrix has numCategories rows and widthOfX columns
    Matrix grad = -y * x.transpose() + pi * x.transpose();
    //We cast the gradient into a vector to make the math easier.
    grad.resize(numCategories * state.widthOfX, 1);

    Matrix GradGradTranspose;
    GradGradTranspose = grad * grad.transpose();
	state.meat += GradGradTranspose;

    /*
         a is a matrix of size JxJ where J is the number of categories
         a_j1j2 = -pi(j1)*(1-pi(j2))if j1 == j2
         a_j1j2 =  pi(j1)*pi(j2) if j1 != j2
    */
    // Compute the 'a' matrix.
    Matrix a(numCategories,numCategories);
    Matrix piDiag = pi.asDiagonal();
    a = pi * pi.transpose() - piDiag;

    //Start the Hessian calculations
    Matrix X_transp_AX(numCategories * state.widthOfX, numCategories * state.widthOfX);

    /*
        Again: The following 3 lines could have been written as
        Matrix XXTrans = x * x.transpose();
        but it creates warnings related to the type of x. Here is an easy fix
    */
    Matrix cv_x = x;
    Matrix XXTrans = trans(cv_x);
    XXTrans = cv_x * XXTrans;

    //Eigen doesn't supported outer-products for matrices, so we have to do our own.
    //This operation is also known as a tensor-product.
    for (int i1 = 0; i1 < state.widthOfX; i1++){
         for (int i2 = 0; i2 <state.widthOfX; i2++){
            int rowOffset = numCategories * i1;
            int colOffset = numCategories * i2;

            X_transp_AX.block(rowOffset, colOffset, numCategories,  numCategories) = XXTrans(i1, i2) * a;
        }
    }

    triangularView<Lower>(state.X_transp_AX) += X_transp_AX;

    return state;

}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
     This merge step is the same as that of logistic regression.
 */
AnyType
mlogregr_robust_step_merge_states::run(AnyType &args) {
    MLogRegrRobustTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    MLogRegrRobustTransitionState<ArrayHandle<double> > stateRight = args[1];

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

AnyType MLrobuststateToResult(
    const Allocator &inAllocator,
    int ref_category, 
    const HandleMap<const ColumnVector, TransparentHandle<double> >& inCoef,
    const ColumnVector &diagonal_of_varianceMat) {

	MutableNativeColumnVector variance(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector stdErr(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector waldZStats(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector waldPValues(
        inAllocator.allocateArray<double>(inCoef.size()));

    for (Index i = 0; i < inCoef.size(); ++i) {
        variance(i) = diagonal_of_varianceMat(i);
        stdErr(i) = std::sqrt(diagonal_of_varianceMat(i));
        waldZStats(i) = inCoef(i) / stdErr(i);
        waldPValues(i) = 2. * prob::cdf(
            prob::normal(), -std::abs(waldZStats(i)));
    }

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple <<  ref_category << inCoef << stdErr << waldZStats << waldPValues;
    return tuple;
}


/**
 * @brief Perform the logistic-regression final step
 */
AnyType
mlogregr_robust_step_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    MLogRegrRobustTransitionState<ArrayHandle<double> > state = args[0];
    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    // See MADLIB-138. At least on certain platforms and with certain versions,
    // LAPACK will run into an infinite loop if pinv() is called for non-finite
    // matrices. We extend the check also to the dependent variables.
    if (!state.X_transp_AX.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
            "calulation. Input data is likely of poor numerical condition.");


    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        -1 * state.X_transp_AX, EigenvaluesOnly, ComputePseudoInverse);

	for(int i = 0; i < state.X_transp_AX.rows(); i++)
	{
		for(int j = 0; j < state.X_transp_AX.cols(); j++)
		{
			elog(INFO, "Bread %i, %i, %f",  i,j, static_cast<float>(state.X_transp_AX(i,j)));
		}
	}

    // Precompute (X^T * A * X)^-1
    Matrix bread = decomposition.pseudoInverse();
	Matrix varianceMat;
    varianceMat = bread * state.meat * bread;

    if(!state.coef.is_finite())
        throw NoSolutionFoundException("Over- or underflow in Newton step, "
            "while updating coefficients. Input data is likely of poor "
            "numerical condition.");

    return MLrobuststateToResult(*this, state.ref_category, state.coef, varianceMat.diagonal());
}

/**
 * @brief Return the difference in log-likelihood between two states
 */
AnyType
__internal_mlogregr_irls_step_distance::run(AnyType &args) {
    MLogRegrIRLSTransitionState<ArrayHandle<double> > stateLeft = args[0];
    MLogRegrIRLSTransitionState<ArrayHandle<double> > stateRight = args[1];

    return std::abs(stateLeft.logLikelihood - stateRight.logLikelihood);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
__internal_mlogregr_irls_result::run(AnyType &args) {
    MLogRegrIRLSTransitionState<ArrayHandle<double> > state = args[0];

    return mLogstateToResult(*this, state.ref_category, state.coef,
        state.gradient, state.logLikelihood, state.X_transp_AX(0,0));
}


/**
 * @brief Compute the diagnostic statistics
 *
 * This function wraps the common parts of computing the results for IRLS.
 */
AnyType mLogstateToResult(
    const Allocator &inAllocator,
    int ref_category,
    const HandleMap<const ColumnVector, TransparentHandle<double> > &inCoef,
    const ColumnVector &diagonal_of_heissian,
    double logLikelihood,
    double conditionNo) {

    MutableNativeColumnVector stdErr(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector waldZStats(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector waldPValues(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector oddsRatios(
        inAllocator.allocateArray<double>(inCoef.size()));

    for (Index i = 0; i < inCoef.size(); ++i) {
        stdErr(i) = std::sqrt(diagonal_of_heissian(i));
        waldZStats(i) = inCoef(i) / stdErr(i);
        waldPValues(i) = 2. * prob::cdf( prob::normal(),
            -std::abs(waldZStats(i)));
        oddsRatios(i) = std::exp( inCoef(i) );
    }

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << ref_category << inCoef << logLikelihood << stdErr << waldZStats << waldPValues
        << oddsRatios << conditionNo;
    return tuple;
}



// ---------------------------------------------------------------------------
//             Marginal Effects Multi-Logistic Regression States
// ---------------------------------------------------------------------------
/**
 * @brief State for marginal effects calculation for logistic regression
 *
 * TransitionState encapsualtes the transition state during the
 * marginal effects calculation for the logistic-regression aggregate function.
 * To the database, the state is exposed as a single DOUBLE PRECISION array,
 * to the C++ code it is a proper object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elemenets are 0.
 *
 */

template <class Handle>
class mlogregrMarginalTransitionState {

    // By §14.5.3/9: "Friend declarations shall not declare partial
    // specializations." We do access protected members in operator+=().
    template <class OtherHandle>
    friend class mlogregrMarginalTransitionState;

public:
    mlogregrMarginalTransitionState(const AnyType &inArray)
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
    inline void initialize(const Allocator &inAllocator, 
        uint16_t inWidthOfX, uint16_t inNumCategories, uint16_t inRefCategory) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
            dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX, inNumCategories));
        rebind(inWidthOfX, inNumCategories);
        widthOfX = inWidthOfX;
        numCategories = inNumCategories;
        ref_category = inRefCategory;
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle> mlogregrMarginalTransitionState &operator=(
        const mlogregrMarginalTransitionState<OtherHandle> &inOtherState) {
        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle> mlogregrMarginalTransitionState &operator+=(
        const mlogregrMarginalTransitionState<OtherHandle> &inOtherState) {
        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");

        numRows += inOtherState.numRows;
        margins_matrix += inOtherState.margins_matrix;
        X_bar += inOtherState.X_bar;
        X_transp_AX += inOtherState.X_transp_AX;
        reference_margins+= inOtherState.reference_margins;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        margins_matrix.fill(0);
        X_bar.fill(0);
        X_transp_AX.fill(0);
        reference_margins.fill(0);
    }

private:
    static inline uint32_t arraySize(const uint16_t inWidthOfX,
        const uint16_t inNumCategories) {
        return 4 + 3*inWidthOfX * inNumCategories + 1*inWidthOfX + 
           inNumCategories * inWidthOfX * inWidthOfX * inNumCategories; 
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX             The number of independent variables.
     * @param inNumCategories The number of categories of the dependant var


     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: widthOfX (number of independant variables)
     * - 1: numCategories (number of categories)
     * - 2: ref_category
     * - 3: coef (vector of coefficients)
     *
     * Intra-iteration components (updated in transition step):
     * - 3 + widthOfX*numCategories: numRows (number of rows already processed in this iteration)
     * - 4 + widthOfX * inNumCategories: margins_matrix 
     */
    void rebind(uint16_t inWidthOfX = 0, uint16_t inNumCategories = 0) {
        widthOfX.rebind(&mStorage[0]);
        numCategories.rebind(&mStorage[1]);
        ref_category.rebind(&mStorage[2]);
        coef.rebind(&mStorage[3], inWidthOfX * inNumCategories);
        numRows.rebind(&mStorage[3 + inWidthOfX * inNumCategories]);
        margins_matrix.rebind(&mStorage[4 + inWidthOfX * inNumCategories],
            inNumCategories , inWidthOfX );
        X_bar.rebind(&mStorage[4 + 2*inWidthOfX * inNumCategories], inWidthOfX);
        reference_margins.rebind(&mStorage[4 + 2*inWidthOfX + 2*inWidthOfX*inNumCategories],
            inWidthOfX);
        X_transp_AX.rebind(&mStorage[4 + 3*inWidthOfX * inNumCategories + 1*inWidthOfX],
            inNumCategories*inWidthOfX, inWidthOfX*inNumCategories);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToUInt16 numCategories;

    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap margins_matrix;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap X_transp_AX;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap reference_margins;
    typename HandleTraits<Handle>::ReferenceToUInt16 ref_category;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap X_bar;
};


AnyType
mlogregr_marginal_step_transition::run(AnyType &args) {
    using std::endl;

	  mlogregrMarginalTransitionState<MutableArrayHandle<double> > state = args[0];
    // Get x as a vector of double
    MappedColumnVector x = args[4].getAs<MappedColumnVector>();
    // Get the category & numCategories as integer
    int16_t category = args[1].getAs<int>();
    // Number of categories after pivoting (We pivot around the first category)
    int16_t numCategories = (args[2].getAs<int>() - 1);
    int32_t ref_category = args[3].getAs<int32_t>();
	  MappedColumnVector coefVec = args[5].getAs<MappedColumnVector>();


    // The following check was added with MADLIB-138.
    if (!x.is_finite())
            throw std::domain_error("Design matrix is not finite.");

    if (state.numRows == 0) {
        if (x.size() > std::numeric_limits<uint16_t>::max())
                throw std::domain_error("Number of independent variables cannot be "
                        "larger than 65535.");
        if (numCategories < 1)
                throw std::domain_error("Number of cateogires must be at least 2");
        if (category > numCategories)
                throw std::domain_error("You have entered a category > numCategories"
                    "Categories must be of values {0,1... numCategories-1}");

        // Init the state (requires x.size() and category.size())
        state.initialize(*this, 
            static_cast<uint16_t>(x.size()) ,
            static_cast<uint16_t>(numCategories),
            static_cast<uint16_t>(ref_category));
                                                    
        state.coef = coefVec;
        state.numCategories = numCategories;
        state.ref_category = ref_category;
    }

    // Now do the transition step
    state.numRows++;
    /*     
        Get y: Convert to 1/0 boolean vector
        Example: Category 4 : 0 0 0 1 0 0
        Storing it in this forms helps us get a nice closed form expression
    */

    //To pivot around the specified reference category
    ColumnVector y(numCategories);
    y.fill(0);
    if (category > ref_category) {
        y(category - 1) = 1;
    } else if (category < ref_category) {
        y(category) = 1;
    }

    
    //    Marginal Effect calculations
    // ----------------------------------------------------------------------
    Matrix coef = state.coef;
    coef.resize(numCategories, state.widthOfX);

    // prob is vector of size # categories
    /* 
        Note: The above 2 lines could have been written as:
        ColumnVector prob = -coef*x; but this creates warnings. 
        See multilog for details. 
    */
    ColumnVector prob(numCategories); 
    prob = coef*x;

    // Calculate the odds ratio
    prob = prob.array().exp();
    double prob_sum = prob.sum();

    prob = prob / (1 + prob_sum);
    
    // Reference category computations. They have been taken out of 
    // the output but left in the infrastructure
    double ref_prob = 1 / (1 + prob_sum);
    
    Matrix probDiag = prob.asDiagonal();


    // Marginal effects (reference calculated separately)
    ColumnVector coef_trans_prob;
    coef_trans_prob = coef.transpose() * prob;
    Matrix margins_matrix = coef;
    margins_matrix.rowwise() -= coef_trans_prob.transpose();
    margins_matrix = probDiag * margins_matrix;


    //    Variance Calculations
    // ----------------------------------------------------------------------
    /*
         a is a matrix of size JxJ where J is the number of categories
         a_j1j2 = -pi(j1)*(1-pi(j2))if j1 == j2
         a_j1j2 =  pi(j1)*pi(j2) if j1 != j2
    */
    Matrix a(numCategories,numCategories);
    a = prob * prob.transpose() - probDiag;

    // Start the Hessian calculations
    Matrix X_transp_AX(numCategories * state.widthOfX, numCategories * state.widthOfX);

    /*
        Again: The following 3 lines could have been written as
        Matrix XXTrans = x * x.transpose();
        but it creates warnings related to the type of x. Here is an easy fix
    */
    Matrix cv_x = x;
    Matrix XXTrans = trans(cv_x);
    XXTrans = cv_x * XXTrans;

    //Eigen doesn't supported outer-products for matrices, so we have to do our own.
    //This operation is also known as a tensor-product.
    for (int i1 = 0; i1 < state.widthOfX; i1++){
         for (int i2 = 0; i2 <state.widthOfX; i2++){
            int rowOffset = numCategories * i1;
            int colOffset = numCategories * i2;

            X_transp_AX.block(rowOffset, colOffset, numCategories,  numCategories) = XXTrans(i1, i2) * a;
        }
    }

    triangularView<Lower>(state.X_transp_AX) += X_transp_AX;
    state.margins_matrix += margins_matrix;
    state.reference_margins += -coef_trans_prob * ref_prob; 
    state.X_bar += x; // It is called X_bar but it really is the sum
    
    return state;

}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
     This merge step is the same as that of logistic regression.
 */
AnyType
mlogregr_marginal_step_merge_states::run(AnyType &args) {
    mlogregrMarginalTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    mlogregrMarginalTransitionState<ArrayHandle<double> > stateRight = args[1];

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

AnyType mlogregr_marginalstateToResult(
    const Allocator &inAllocator,
    const int numRows,
    const ColumnVector &inCoef,
    const ColumnVector &inMargins,
    const ColumnVector &inVariance
    ) {

    
    MutableNativeColumnVector margins(
        inAllocator.allocateArray<double>(inMargins.size()));
    MutableNativeColumnVector coef(
        inAllocator.allocateArray<double>(inMargins.size()));
    MutableNativeColumnVector stdErr(
        inAllocator.allocateArray<double>(inMargins.size()));
    MutableNativeColumnVector tStats(
        inAllocator.allocateArray<double>(inMargins.size()));
    MutableNativeColumnVector pValues(
        inAllocator.allocateArray<double>(inMargins.size()));

    for (Index i = 0; i < inMargins.size(); ++i) {
        margins(i) = inMargins(i);
        coef(i) = inCoef(i);
        stdErr(i) = std::sqrt(inVariance(i));
        tStats(i) = margins(i) / stdErr(i);

        // P-values only make sense if numRows > coef.size()
        if (numRows > inCoef.size())
          pValues(i) = 2. * prob::cdf(
              boost::math::complement(
                  prob::students_t(
                      static_cast<double>(numRows - inCoef.size())
                  ),
                  std::fabs(tStats(i))
              ));
    }

    // Return all coefficients, standard errors, etc. in a tuple
    // Note: PValues will return NULL if numRows <= coef.size
    AnyType tuple;
    tuple << margins
          << coef
          << stdErr
          << tStats
        	<< (numRows > inCoef.size()? pValues: Null());
    return tuple;
}


/**
 * @brief Perform the logistic-regression final step
 */
AnyType
mlogregr_marginal_step_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    mlogregrMarginalTransitionState<ArrayHandle<double> > state = args[0];
    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    if(!state.coef.is_finite())
        throw NoSolutionFoundException("Over- or underflow in Newton step, "
            "while updating coefficients. Input data is likely of poor "
            "numerical condition.");

    // See MADLIB-138. At least on certain platforms and with certain versions,
    // LAPACK will run into an infinite loop if pinv() is called for non-finite
    // matrices. We extend the check also to the dependent variables.
    if (!state.X_transp_AX.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
            "calulation. Input data is likely of poor numerical condition.");

    // Include marginal effects of reference variable: 
    // FIXME: They have been taken out of the output for now
    //const int size = state.coef.size() + numIndepVars;
    const int size = state.coef.size();
    
    // Variance-covariance calculation
    // ----------------------------------------------------------
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        -1 * state.X_transp_AX, EigenvaluesOnly, ComputePseudoInverse);

    // Precompute -(X^T * A * X)^-1
    Matrix V = decomposition.pseudoInverse();



    // Marginal Effect calculation
    // ---------------------------------------------------------
    Matrix margins_matrix;
    margins_matrix = state.margins_matrix / state.numRows;


    // Standard error calculation
    // ----------------------------------------------------------
    ColumnVector x_bar = state.X_bar / state.numRows;
    int numIndepVars = state.coef.size() / state.numCategories;
    int numCategories = state.numCategories;
    
    Matrix coef = state.coef;
    coef.resize(state.numCategories, state.widthOfX);
    
    // Variance & Marginal gradient
    ColumnVector marginal_gradient(size);
    ColumnVector variance(size);
    variance.setOnes();

    // Probibility vector at the mean
    ColumnVector p_bar(state.numCategories); 
    ColumnVector coef_x_bar(state.numCategories);
    ColumnVector coef_trans_p_bar(state.widthOfX);
    
    coef_x_bar = coef*x_bar;
    coef_trans_p_bar = coef.transpose() * p_bar;

    p_bar = coef_x_bar;
    p_bar = p_bar.array().exp();
    p_bar = p_bar / (1 + p_bar.sum());

    // Marginal effects at the mean
    Matrix margins_mean_matrix(numCategories, numIndepVars);
    margins_mean_matrix.rowwise() -= coef_trans_p_bar.transpose();
    margins_mean_matrix = p_bar.asDiagonal() * margins_matrix;

    // Compute the variance for each marginal effect
    int index, e_j_J, e_k_K, e_k_K_j_J;
    for (int K=0; K < numIndepVars; K++){
      for (int J=0; J < numCategories; J++){

        for (int k=0; k < numIndepVars; k++){
          for (int j=0; j < numCategories; j++){
            
            e_j_J = (j==J) ? 1: 0;
            e_k_K = (k==K) ? 1: 0;
            e_k_K_j_J = (j==J && k==K) ? 1: 0;

            index = k*numCategories + j;

            marginal_gradient(index) = 
                x_bar(k) * (e_j_J  - p_bar(j)) * margins_mean_matrix(J,K);

            marginal_gradient(index) += p_bar(J) * 
                ( e_k_K_j_J - p_bar(j) * e_k_K - x_bar(k) * margins_mean_matrix(j,K));
          }
        }

        // NOTE: Since the earlier Variance calculations are being done by
        // stacking up the indepdent variables for each category separtely
        variance(K + numIndepVars * J) = 
                          marginal_gradient.transpose() * V * marginal_gradient;

      }
    }

    

    // Add in reference variables to all the calculations
    // ----------------------------------------------------------
    ColumnVector coef_with_ref(size);
    ColumnVector margins_with_ref(size);

    // Vectorize the margins_matrix and add the reference variable
    for (int j=0; j < numCategories; j++){
      for (int k=0; k < numIndepVars; k++){
        
        index = k + numIndepVars *j;
        coef_with_ref(index) = coef(j,k);

        index = k + numIndepVars*j;
        margins_with_ref(index) = margins_matrix(j,k);
      }
    }

    return mlogregr_marginalstateToResult(*this, 
                                          state.numRows,
                                          coef_with_ref, 
                                          margins_with_ref,
                                          variance);
}


// ------------------------ End of Marginal ------------------------------------


} // namespace regress

} // namespace modules

} // namespace madlib
