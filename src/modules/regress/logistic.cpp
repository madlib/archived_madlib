/* ----------------------------------------------------------------------- *//**
 *
 * @file logistic.cpp
 *
 * @brief Logistic-Regression functions
 *
 * We implement the conjugate-gradient method and the iteratively-reweighted-
 * least-squares method.
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>

#include "logistic.hpp"

namespace madlib {

// Use Eigen
using namespace dbal::eigen_integration;

namespace modules {

// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

namespace regress {

// Internal functions
AnyType stateToResult(const Allocator &inAllocator,
    const HandleMap<const ColumnVector, TransparentHandle<double> > &coef,
    const ColumnVector &diagonal_of_inverse_of_X_transp_AX,
    double logLikelihood,
    double conditionNo);

/**
 * @brief Inter- and intra-iteration state for conjugate-gradient method for
 *        logistic regression
 *
 * TransitionState encapsualtes the transition state during the
 * logistic-regression aggregate function. To the database, the state is
 * exposed as a single DOUBLE PRECISION array, to the C++ code it is a proper
 * object containing scalars and vectors.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elemenets are 0.
 *
 */
template <class Handle>
class LogRegrCGTransitionState {
    template <class OtherHandle>
    friend class LogRegrCGTransitionState;

public:
    LogRegrCGTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[1]));
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
     * @brief Initialize the conjugate-gradient state.
     *
     * This function is only called for the first iteration, for the first row.
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
    LogRegrCGTransitionState &operator=(
        const LogRegrCGTransitionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    LogRegrCGTransitionState &operator+=(
        const LogRegrCGTransitionState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");

        numRows += inOtherState.numRows;
        gradNew += inOtherState.gradNew;
        X_transp_AX += inOtherState.X_transp_AX;
        logLikelihood += inOtherState.logLikelihood;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        X_transp_AX.fill(0);
        gradNew.fill(0);
        logLikelihood = 0;
    }

private:
    static inline size_t arraySize(const uint16_t inWidthOfX) {
        return 5 + inWidthOfX * inWidthOfX + 4 * inWidthOfX;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX The number of independent variables.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: iteration (current iteration)
     * - 1: widthOfX (number of coefficients)
     * - 2: coef (vector of coefficients)
     * - 2 + widthOfX: dir (direction)
     * - 2 + 2 * widthOfX: grad (gradient)
     * - 2 + 3 * widthOfX: beta (scale factor)
     *
     * Intra-iteration components (updated in transition step):
     * - 3 + 3 * widthOfX: numRows (number of rows already processed in this iteration)
     * - 4 + 3 * widthOfX: gradNew (intermediate value for gradient)
     * - 4 + 4 * widthOfX: X_transp_AX (X^T A X)
     * - 4 + widthOfX * widthOfX + 4 * widthOfX: logLikelihood ( ln(l(c)) )
     */
    void rebind(uint16_t inWidthOfX) {
        iteration.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        coef.rebind(&mStorage[2], inWidthOfX);
        dir.rebind(&mStorage[2 + inWidthOfX], inWidthOfX);
        grad.rebind(&mStorage[2 + 2 * inWidthOfX], inWidthOfX);
        beta.rebind(&mStorage[2 + 3 * inWidthOfX]);
        numRows.rebind(&mStorage[3 + 3 * inWidthOfX]);
        gradNew.rebind(&mStorage[4 + 3 * inWidthOfX], inWidthOfX);
        X_transp_AX.rebind(&mStorage[4 + 4 * inWidthOfX], inWidthOfX, inWidthOfX);
        logLikelihood.rebind(&mStorage[4 + inWidthOfX * inWidthOfX + 4 * inWidthOfX]);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt32 iteration;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap dir;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap grad;
    typename HandleTraits<Handle>::ReferenceToDouble beta;

    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap gradNew;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap X_transp_AX;
    typename HandleTraits<Handle>::ReferenceToDouble logLikelihood;
};

/**
 * @brief Logistic function
 */
inline double sigma(double x) {
	return 1. / (1. + std::exp(-x));
}

/**
 * @brief Perform the logistic-regression transition step
 */
AnyType
logregr_cg_step_transition::run(AnyType &args) {
    LogRegrCGTransitionState<MutableArrayHandle<double> > state = args[0];
    double y = args[1].getAs<bool>() ? 1. : -1.;
    HandleMap<const ColumnVector> x = args[2].getAs<ArrayHandle<double> >();

    // The following check was added with MADLIB-138.
    if (!isfinite(x))
        throw std::domain_error("Design matrix is not finite.");

    if (state.numRows == 0) {
        if (x.size() > std::numeric_limits<uint16_t>::max())
            throw std::domain_error("Number of independent variables cannot be "
                "larger than 65535.");

        state.initialize(*this, static_cast<uint16_t>(x.size()));
        if (!args[3].isNull()) {
            LogRegrCGTransitionState<ArrayHandle<double> > previousState = args[3];

            state = previousState;
            state.reset();
        }
    }

    // Now do the transition step
    state.numRows++;
    double xc = dot(x, state.coef);
    state.gradNew.noalias() += sigma(-y * xc) * y * trans(x);

    // Note: sigma(-x) = 1 - sigma(x).
    // a_i = sigma(x_i c) sigma(-x_i c)
    double a = sigma(xc) * sigma(-xc);
    triangularView<Lower>(state.X_transp_AX) += x * trans(x) * a;

    //          n
    //         --
    // l(c) = -\  log(1 + exp(-y_i * c^T x_i))
    //         /_
    //         i=1
    state.logLikelihood -= std::log( 1. + std::exp(-y * xc) );

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
logregr_cg_step_merge_states::run(AnyType &args) {
    LogRegrCGTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    LogRegrCGTransitionState<ArrayHandle<double> > stateRight = args[1];

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
logregr_cg_step_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    LogRegrCGTransitionState<MutableArrayHandle<double> > state = args[0];

    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    // Note: k = state.iteration
    if (state.iteration == 0) {
		// Iteration computes the gradient

		state.dir = state.gradNew;
		state.grad = state.gradNew;
	} else {
        // We use the Hestenes-Stiefel update formula:
        //
		//            g_k^T (g_k - g_{k-1})
		// beta_k = -------------------------
		//          d_{k-1}^T (g_k - g_{k-1})
        ColumnVector gradNewMinusGrad = state.gradNew - state.grad;
        state.beta
            = dot(state.gradNew, gradNewMinusGrad)
            / dot(state.dir, gradNewMinusGrad);

        // Alternatively, we could use Polak-Ribière
        // state.beta
        //     = dot(state.gradNew, gradNewMinusGrad)
        //     / dot(state.grad, state.grad);

        // Or Fletcher–Reeves
        // state.beta
        //     = dot(state.gradNew, state.gradNew)
        //     / dot(state.grad, state.grad);

        // Do a direction restart (Powell restart)
        // Note: This is testing whether state.beta < 0 if state.beta were
        // assigned according to Polak-Ribière
        if (dot(state.gradNew, gradNewMinusGrad)
            / dot(state.grad, state.grad) < 0) state.beta = 0;

        // d_k = g_k - beta_k * d_{k-1}
        state.dir = state.gradNew - state.beta * state.dir;
		state.grad = state.gradNew;
	}

    // H_k = - X^T A_k X
    // where A_k = diag(a_1, ..., a_n) and a_i = sigma(x_i c_{k-1}) sigma(-x_i c_{k-1})
    //
    //             g_k^T d_k
    // alpha_k = -------------
    //           d_k^T H_k d_k
    //
    // c_k = c_{k-1} - alpha_k * d_k
    state.coef += dot(state.grad, state.dir) /
        as_scalar(trans(state.dir) * state.X_transp_AX * state.dir)
        * state.dir;

    if(!state.coef.is_finite())
        throw NoSolutionFoundException("Over- or underflow in "
            "conjugate-gradient step, while updating coefficients. Input data "
            "is likely of poor numerical condition.");

    state.iteration++;
    return state;
}

/**
 * @brief Return the difference in log-likelihood between two states
 */
AnyType
internal_logregr_cg_step_distance::run(AnyType &args) {
    LogRegrCGTransitionState<ArrayHandle<double> > stateLeft = args[0];
    LogRegrCGTransitionState<ArrayHandle<double> > stateRight = args[1];

    return std::abs(stateLeft.logLikelihood - stateRight.logLikelihood);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_logregr_cg_result::run(AnyType &args) {
    LogRegrCGTransitionState<ArrayHandle<double> > state = args[0];

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.X_transp_AX, EigenvaluesOnly, ComputePseudoInverse);

    return stateToResult(*this, state.coef,
        decomposition.pseudoInverse().diagonal(), state.logLikelihood,
        decomposition.conditionNo());
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
class LogRegrIRLSTransitionState {
    template <class OtherHandle>
    friend class LogRegrIRLSTransitionState;

public:
    LogRegrIRLSTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[0]));
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
    LogRegrIRLSTransitionState &operator=(
        const LogRegrIRLSTransitionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    LogRegrIRLSTransitionState &operator+=(
        const LogRegrIRLSTransitionState<OtherHandle> &inOtherState) {

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
    static inline uint32_t arraySize(const uint16_t inWidthOfX) {
        return 3 + inWidthOfX * inWidthOfX + 2 * inWidthOfX;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX The number of independent variables.
     *
     * Array layout (iteration refers to one aggregate-function call):
     * Inter-iteration components (updated in final function):
     * - 0: widthOfX (number of coefficients)
     * - 1: coef (vector of coefficients)
     *
     * Intra-iteration components (updated in transition step):
     * - 1 + widthOfX: numRows (number of rows already processed in this iteration)
     * - 2 + widthOfX: X_transp_Az (X^T A z)
     * - 2 + 2 * widthOfX: X_transp_AX (X^T A X)
     * - 2 + widthOfX^2 + 2 * widthOfX: logLikelihood ( ln(l(c)) )
     */
    void rebind(uint16_t inWidthOfX = 0) {
        widthOfX.rebind(&mStorage[0]);
        coef.rebind(&mStorage[1], inWidthOfX);
        numRows.rebind(&mStorage[1 + inWidthOfX]);
        X_transp_Az.rebind(&mStorage[2 + inWidthOfX], inWidthOfX);
        X_transp_AX.rebind(&mStorage[2 + 2 * inWidthOfX], inWidthOfX, inWidthOfX);
        logLikelihood.rebind(&mStorage[2 + inWidthOfX * inWidthOfX + 2 * inWidthOfX]);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;

    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap X_transp_Az;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap X_transp_AX;
    typename HandleTraits<Handle>::ReferenceToDouble logLikelihood;
};

AnyType
logregr_irls_step_transition::run(AnyType &args) {
    LogRegrIRLSTransitionState<MutableArrayHandle<double> > state = args[0];
    double y = args[1].getAs<bool>() ? 1. : -1.;
    HandleMap<const ColumnVector> x = args[2].getAs<ArrayHandle<double> >();

    // The following check was added with MADLIB-138.
    if (!x.is_finite())
        throw std::domain_error("Design matrix is not finite.");

    if (state.numRows == 0) {
        if (x.size() > std::numeric_limits<uint16_t>::max())
            throw std::domain_error("Number of independent variables cannot be "
                "larger than 65535.");

        state.initialize(*this, static_cast<uint16_t>(x.size()));
        if (!args[3].isNull()) {
            LogRegrIRLSTransitionState<ArrayHandle<double> > previousState = args[3];

            state = previousState;
            state.reset();
        }
    }

    // Now do the transition step
    state.numRows++;

    // xc = x^T_i c
    double xc = dot(x, state.coef);

    // a_i = sigma(x_i c) sigma(-x_i c)
    double a = sigma(xc) * sigma(-xc);

    // Note: sigma(-x) = 1 - sigma(x).
    //
    //             sigma(-y_i x_i c) y_i
    // z = x_i c + ---------------------
    //                     a_i
    //
    // To avoid overflows if a_i is close to 0, we do not compute z directly,
    // but instead compute a * z.
    double az = xc * a + sigma(-y * xc) * y;

    state.X_transp_Az.noalias() += x * az;
    triangularView<Lower>(state.X_transp_AX) += x * trans(x) * a;

    //          n
    //         --
    // l(c) = -\  ln(1 + exp(-y_i * c^T x_i))
    //         /_
    //         i=1
    state.logLikelihood -= std::log( 1. + std::exp(-y * xc) );
    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
logregr_irls_step_merge_states::run(AnyType &args) {
    LogRegrIRLSTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    LogRegrIRLSTransitionState<ArrayHandle<double> > stateRight = args[1];

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
logregr_irls_step_final::run(AnyType &args) {
    // We request a mutable object. Depending on the backend, this might perform
    // a deep copy.
    LogRegrIRLSTransitionState<MutableArrayHandle<double> > state = args[0];

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
internal_logregr_irls_step_distance::run(AnyType &args) {
    LogRegrIRLSTransitionState<ArrayHandle<double> > stateLeft = args[0];
    LogRegrIRLSTransitionState<ArrayHandle<double> > stateRight = args[1];

    return std::abs(stateLeft.logLikelihood - stateRight.logLikelihood);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_logregr_irls_result::run(AnyType &args) {
    LogRegrIRLSTransitionState<ArrayHandle<double> > state = args[0];

    return stateToResult(*this, state.coef,
        state.X_transp_Az, state.logLikelihood, state.X_transp_AX(0,0));
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
 */
template <class Handle>
class LogRegrIGDTransitionState {
    template <class OtherHandle>
    friend class LogRegrIGDTransitionState;

public:
    LogRegrIGDTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[0]));
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
     * @brief Initialize the conjugate-gradient state.
     *
     * This function is only called for the first iteration, for the first row.
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
    LogRegrIGDTransitionState &operator=(
        const LogRegrIGDTransitionState<OtherHandle> &inOtherState) {

        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    LogRegrIGDTransitionState &operator+=(
        const LogRegrIGDTransitionState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error("Internal error: Incompatible transition "
                "states");

		// Compute the average of the models. Note: The following remains an
        // invariant, also after more than one merge:
        // The model is a linear combination of the per-segment models
        // where the coefficient (weight) for each per-segment model is the
        // ratio "# rows in segment / total # rows of all segments merged so
        // far".
		double totalNumRows = static_cast<double>(numRows)
                            + static_cast<double>(inOtherState.numRows);
		coef = double(numRows) / totalNumRows * coef
			+ double(inOtherState.numRows) / totalNumRows * inOtherState.coef;

        numRows += inOtherState.numRows;
        X_transp_AX += inOtherState.X_transp_AX;
        logLikelihood += inOtherState.logLikelihood;
        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
		// FIXME: HAYING: stepsize if hard-coded here now
        stepsize = .1;
        numRows = 0;
        X_transp_AX.fill(0);
        logLikelihood = 0;
    }

private:
    static inline uint32_t arraySize(const uint16_t inWidthOfX) {
        return 4 + inWidthOfX * inWidthOfX + inWidthOfX;
    }
    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX The number of independent variables.
     *
     * Array layout (iteration refers to one aggregate-function call):
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
    void rebind(uint16_t inWidthOfX) {
        widthOfX.rebind(&mStorage[0]);
        stepsize.rebind(&mStorage[1]);
        coef.rebind(&mStorage[2], inWidthOfX);
        numRows.rebind(&mStorage[2 + inWidthOfX]);
        X_transp_AX.rebind(&mStorage[3 + inWidthOfX], inWidthOfX, inWidthOfX);
        logLikelihood.rebind(&mStorage[3 + inWidthOfX * inWidthOfX + inWidthOfX]);
    }

    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToDouble stepsize;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;

    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
	typename HandleTraits<Handle>::MatrixTransparentHandleMap X_transp_AX;
    typename HandleTraits<Handle>::ReferenceToDouble logLikelihood;
};

AnyType
logregr_igd_step_transition::run(AnyType &args) {
    LogRegrIGDTransitionState<MutableArrayHandle<double> > state = args[0];
    double y = args[1].getAs<bool>() ? 1. : -1.;
    HandleMap<const ColumnVector> x = args[2].getAs<ArrayHandle<double> >();

    // The following check was added with MADLIB-138.
    if (!x.is_finite())
        throw std::domain_error("Design matrix is not finite.");

	// We only know the number of independent variables after seeing the first
    // row.
    if (state.numRows == 0) {
        if (x.size() > std::numeric_limits<uint16_t>::max())
            throw std::domain_error("Number of independent variables cannot be "
                "larger than 65535.");

        state.initialize(*this, static_cast<uint16_t>(x.size()));

		// For the first iteration, the previous state is NULL
        if (!args[3].isNull()) {
			LogRegrIGDTransitionState<ArrayHandle<double> > previousState = args[3];

            state = previousState;
            state.reset();
        }
    }

    // Now do the transition step
    state.numRows++;

    // xc = x^T_i c
    double xc = dot(x, state.coef);
    double scale = state.stepsize * sigma(-xc * y) * y;
	state.coef += scale * x;

    // Note: previous coefficients are used for Hessian and log likelihood
	if (!args[3].isNull()) {
		LogRegrIGDTransitionState<ArrayHandle<double> > previousState = args[3];

		double previous_xc = dot(x, previousState.coef);

        // a_i = sigma(x_i c) sigma(-x_i c)
		double a = sigma(previous_xc) * sigma(-previous_xc);
		triangularView<Lower>(state.X_transp_AX) += x * trans(x) * a;

		// l_i(c) = - ln(1 + exp(-y_i * c^T x_i))
		state.logLikelihood -= std::log( 1. + std::exp(-y * previous_xc) );
	}

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
logregr_igd_step_merge_states::run(AnyType &args) {
    LogRegrIGDTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    LogRegrIGDTransitionState<ArrayHandle<double> > stateRight = args[1];

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
 *
 * All that we do here is to test whether we have seen any data. If not, we
 * return NULL. Otherwise, we return the transition state unaltered.
 */
AnyType
logregr_igd_step_final::run(AnyType &args) {
    LogRegrIRLSTransitionState<ArrayHandle<double> > state = args[0];

    if(!state.coef.is_finite())
        throw NoSolutionFoundException("Over- or underflow in "
            "incremental-gradient iteration. Input data is likely of poor "
            "numerical condition.");

    // Aggregates that haven't seen any data just return Null.
    if (state.numRows == 0)
        return Null();

    return state;
}

/**
 * @brief Return the difference in log-likelihood between two states
 */
AnyType
internal_logregr_igd_step_distance::run(AnyType &args) {
    LogRegrIGDTransitionState<ArrayHandle<double> > stateLeft = args[0];
    LogRegrIGDTransitionState<ArrayHandle<double> > stateRight = args[1];

    return std::abs(stateLeft.logLikelihood - stateRight.logLikelihood);
}

/**
 * @brief Return the coefficients and diagnostic statistics of the state
 */
AnyType
internal_logregr_igd_result::run(AnyType &args) {
    LogRegrIGDTransitionState<ArrayHandle<double> > state = args[0];

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.X_transp_AX, EigenvaluesOnly, ComputePseudoInverse);

    return stateToResult(*this, state.coef,
        decomposition.pseudoInverse().diagonal(), state.logLikelihood,
        decomposition.conditionNo());
}

/**
 * @brief Compute the diagnostic statistics
 *
 * This function wraps the common parts of computing the results for both the
 * CG and the IRLS method.
 */
AnyType stateToResult(
    const Allocator &inAllocator,
    const HandleMap<const ColumnVector, TransparentHandle<double> > &inCoef,
    const ColumnVector &diagonal_of_inverse_of_X_transp_AX,
    double logLikelihood,
    double conditionNo) {

    // FIXME: We currently need to copy the coefficient to a native array
    // This should be transparent to user code
    HandleMap<ColumnVector> coef(
        inAllocator.allocateArray<double>(inCoef.size()));
    coef = inCoef;

    HandleMap<ColumnVector> stdErr(
        inAllocator.allocateArray<double>(coef.size()));
    HandleMap<ColumnVector> waldZStats(
        inAllocator.allocateArray<double>(coef.size()));
    HandleMap<ColumnVector> waldPValues(
        inAllocator.allocateArray<double>(coef.size()));
    HandleMap<ColumnVector> oddsRatios(
        inAllocator.allocateArray<double>(coef.size()));

    for (Index i = 0; i < coef.size(); ++i) {
        stdErr(i) = std::sqrt(diagonal_of_inverse_of_X_transp_AX(i));
        waldZStats(i) = coef(i) / stdErr(i);
        waldPValues(i) = 2. * prob::cdf( prob::normal(),
            -std::abs(waldZStats(i)));
        oddsRatios(i) = std::exp( coef(i) );
    }

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << coef << logLikelihood << stdErr << waldZStats << waldPValues
        << oddsRatios << conditionNo;
    return tuple;
}

} // namespace regress

} // namespace modules

} // namespace madlib
