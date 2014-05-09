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

    if (args[1].isNull() || args[2].isNull() || args[3].isNull() ||
            args[4].isNull()) {
        return args[0];
    }

    // Get x as a vector of double
    MappedColumnVector x;
    try{
        // an exception is raised in the backend if args[2] contains nulls
        MappedColumnVector xx = args[4].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    // Get the category & numCategories as integer
    int32_t category = args[1].getAs<int32_t>();
    // Number of categories after pivoting (we pivot around the first category)
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
    Matrix hessianInv = -1 * decomposition.pseudoInverse();

    state.coef.noalias() += hessianInv * state.gradient;

    if(!state.coef.is_finite())
        throw NoSolutionFoundException("Over- or underflow in Newton step, "
            "while updating coefficients. Input data is likely of poor "
            "numerical condition.");

    // We use the intra-iteration field gradient for storing the diagonal
    // of X^T A X, so that we don't have to recompute it in the result function.
    // Likewise, we store the condition number.
    // FIXME: This feels a bit like a hack.
    state.gradient = -1 * hessianInv.diagonal();
    state.X_transp_AX(0,0) = decomposition.conditionNo();

    return state;
}

/**
 * @brief Compute the diagnostic statistics
 *
 * This function wraps the common parts of mlogregr state into the result.
 * (the result data type is defined in multilogistic.sql_in)
 */
AnyType mLogstateToResult(
    const Allocator &inAllocator,
    MLogRegrIRLSTransitionState<ArrayHandle<double> > state) {

    int ref_category = state.ref_category;
    const HandleMap<const ColumnVector, TransparentHandle<double> > &inCoef = state.coef;
    const ColumnVector &diagonal_of_hessian = state.gradient;
    double logLikelihood = state.logLikelihood;
    double conditionNo = state.X_transp_AX(0,0);
    int num_processed = state.numRows;

    MutableNativeColumnVector stdErr(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector waldZStats(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector waldPValues(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector oddsRatios(
        inAllocator.allocateArray<double>(inCoef.size()));

    for (Index i = 0; i < inCoef.size(); ++i) {
        stdErr(i) = std::sqrt(diagonal_of_hessian(i));
        waldZStats(i) = inCoef(i) / stdErr(i);
        waldPValues(i) = 2. * prob::cdf( prob::normal(),
            -std::abs(waldZStats(i)));
        oddsRatios(i) = std::exp( inCoef(i) );
    }
    int num_iterations = 0;
    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << ref_category << inCoef << logLikelihood << stdErr
          << waldZStats << waldPValues << oddsRatios
          << conditionNo << num_iterations << num_processed;
    return tuple;
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

    return mLogstateToResult(*this, state);
    // state.ref_category, state.coef,
    // state.gradient, state.logLikelihood, state.X_transp_AX(0,0));
}

// ----------------- End of Multinomial Logistic Regression --------------------

// ---------------------------------------------------------------------------
//             Robust Variance Multi-Logistic
// ---------------------------------------------------------------------------

AnyType
mlogregr_robust_step_transition::run(AnyType &args) {
    using std::endl;

	MLogRegrRobustTransitionState<MutableArrayHandle<double> > state = args[0];

   if (args[1].isNull() || args[2].isNull() || args[3].isNull() ||
            args[4].isNull() || args[5].isNull()) {
        return args[0];
    }

    // Get x as a vector of double
    MappedColumnVector x;
    try{
        // an exception is raised in the backend if args[2] contains nulls
        MappedColumnVector xx = args[4].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    // Get the category & numCategories as integer
    int16_t category = args[1].getAs<int>();
    // Number of categories after pivoting (We pivot around the first category)
    int16_t numCategories = (args[2].getAs<int>() - 1);
    int32_t ref_category = args[3].getAs<int32_t>();
	MappedMatrix coefMat = args[5].getAs<MappedMatrix>();

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
       
        Matrix mat = coefMat;
        mat.transposeInPlace();
        mat.resize(coefMat.size(), 1);
        state.coef = mat;
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
// ------------------------ End of Robust Variance -----------------------------

AnyType __sub_array::run(AnyType &args) {
    if (args[0].isNull() || args[1].isNull())
        return Null();

    ArrayHandle<double> value = args[0].getAs<ArrayHandle<double> >();
    ArrayHandle<int32> index = args[1].getAs<ArrayHandle<int32> >();

    for (size_t i = 0; i < index.size(); i++) {
        if (index[i] < 1 || index[i] > static_cast<int>(value.size()))
            throw std::domain_error("Invalid indices - out of bound");
    }

    MutableArrayHandle<double> res =
        allocateArray<double, dbal::AggregateContext, dbal::DoZero,
            dbal::ThrowBadAlloc>(index.size());

    for (size_t i = 0; i < index.size(); i++)
        res[i] = value[index[i] - 1];

    return res;
}

// ----------------------------------------------------------------------------
typedef struct __sr_ctx{
    const double * inarray;
    int32_t maxcall;
    int32_t num_feature;
    int32_t num_category;
    int32_t ref_category;
    int32_t curcall;
} sr_ctx;

void * __mlogregr_format::SRF_init(AnyType &args) 
{
    sr_ctx * ctx = new sr_ctx;

    if(args[0].isNull() || args[1].isNull() ||
       args[2].isNull() || args[3].isNull()){
        ctx->maxcall = 1;
        ctx->curcall = -1; 
        return ctx;
    }

    MutableArrayHandle<double> inarray = NULL;
    try{
        inarray = args[0].getAs<MutableArrayHandle<double> >();
    } catch (const ArrayWithNullException &e) {
        ctx->maxcall = 0;
        return ctx;
    }
    
    int32_t num_feature = args[1].getAs<int32_t>();
    int32_t num_category = args[2].getAs<int32_t>();
    int32_t ref_category = args[3].getAs<int32_t>();

    ctx->inarray = inarray.ptr();
    ctx->maxcall = num_category - 1;
    ctx->num_category = num_category - 1;
    ctx->num_feature = num_feature;
    ctx->ref_category = ref_category;
    ctx->curcall = 0;

    if(num_feature * (num_category - 1) != static_cast<int32_t>(inarray.size())){
        ctx->maxcall = 0;
    }

    if(ref_category >= num_category){
        ctx->maxcall = 0;
    }

    return ctx;
}

AnyType __mlogregr_format::SRF_next(void * user_fctx, bool * is_last_call)
{
    sr_ctx * ctx = (sr_ctx *) user_fctx;
    if (ctx->maxcall == 0) {
        *is_last_call = true;
        return Null();
    }
    
    if(ctx->maxcall == 1 and ctx->curcall == -1){
        ctx->maxcall =0;
        return Null();
    }

    try{
        MutableArrayHandle<double> outarray =
            allocateArray<double, dbal::FunctionContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(ctx->num_feature);
        for(int i = 0; i < ctx->num_feature; i++)
            outarray[i] = ctx->inarray[i * ctx->num_category + ctx->curcall];

        AnyType tuple;
        tuple << (ctx->curcall < ctx->ref_category ? ctx->curcall : ctx->curcall + 1) << outarray;

        ctx->curcall++;
        ctx->maxcall--;

        return tuple; 
    }catch(...){
        ctx->maxcall = 0;
        return Null();
    }

}

} // namespace regress

} // namespace modules

} // namespace madlib
