/* ----------------------------------------------------------------------
   Robust Variance estimator for CoxPH model
   ---------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>

#include "robust_variance_coxph.hpp"

namespace madlib {
namespace modules {
namespace stats {

// Use Eigen
using namespace dbal::eigen_integration;
// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

using namespace std;

// ----------------------------------------------------------------------

/**
 * @brief Transition state for the CoxPH robust variance
 */
template <class Handle>
class RBCoxPHTransitionState {

    template <class OtherHandle>
    friend class RBCoxPHTransitionState;

  public:
    RBCoxPHTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[1]));
    }

    /**
     * @brief Convert to backend representation
     *
     * We define this function so that we can use TransitionState in the argument
     * list and as a return type.   */
    inline operator AnyType() const {
        return mStorage;
    }

    /**
     * @brief Initialize the transition state. Only called for first row.
     *
     * @param inAllocator Allocator for the memory transition state. Must fill
     *     the memory block with zeros.
     * @param inWidthOfX Number of independent variables. The first row of data
     *     determines the size of the transition state. This size is a quadratic
     *     function of inWidthOfX.
     */
    inline void initialize(const Allocator &inAllocator, uint16_t inWidthOfX,
                           const double * inCoef = 0, const double* inHessian = 0)
    {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                                             dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX));
        rebind(inWidthOfX);
        widthOfX = inWidthOfX;

        if(inCoef){
            for(uint16_t i = 0; i < widthOfX; i++)
                coef[i] = inCoef[i];
        }

        if (inHessian) {
            int count = 0;
            for (uint16_t i = 0; i < widthOfX; i++)
                for (uint16_t j = 0; j < widthOfX; j++) {
                    hessian(j,i) = inHessian[count++];
                }
        }
        this->reset();
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        A = 0;
        y_previous = 0;
        multiplier = 0;
        B.fill(0);
        M.fill(0);
        tie12.setZero();
        tie13.setZero();
        tie23.setZero();
        tie22.setZero();
        tie33 = 0;
    }

  private:
    static inline size_t arraySize(const uint16_t inWidthOfX) {
        return 7 + 4 * inWidthOfX + 4 * inWidthOfX * inWidthOfX;
    }

    void rebind(uint16_t inWidthOfX) {
        numRows.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        multiplier.rebind(&mStorage[2]);
        y_previous.rebind(&mStorage[3]);
        coef.rebind(&mStorage[4], inWidthOfX);
        A.rebind(&mStorage[4+inWidthOfX]);
        B.rebind(&mStorage[5+inWidthOfX], inWidthOfX);
        M.rebind(&mStorage[5+2*inWidthOfX],
                 inWidthOfX, inWidthOfX);
        hessian.rebind(&mStorage[5+2*inWidthOfX+inWidthOfX*inWidthOfX],
                       inWidthOfX, inWidthOfX);
        tie12.rebind(&mStorage[5+2*inWidthOfX+2*inWidthOfX*inWidthOfX], inWidthOfX, inWidthOfX);
        tie13.rebind(&mStorage[5+2*inWidthOfX+3*inWidthOfX*inWidthOfX], inWidthOfX);
        tie23.rebind(&mStorage[5+3*inWidthOfX+3*inWidthOfX*inWidthOfX], inWidthOfX);
        tie22.rebind(&mStorage[5+4*inWidthOfX+3*inWidthOfX*inWidthOfX], inWidthOfX, inWidthOfX);
        tie33.rebind(&mStorage[5+4*inWidthOfX+4*inWidthOfX*inWidthOfX]);
        num_censored.rebind(&mStorage[6+4*inWidthOfX+4*inWidthOfX*inWidthOfX]);
    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToDouble multiplier;
    typename HandleTraits<Handle>::ReferenceToDouble y_previous;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;

    typename HandleTraits<Handle>::ReferenceToDouble A;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap B;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap M; // meat
    typename HandleTraits<Handle>::MatrixTransparentHandleMap hessian;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap tie12;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap tie13;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap tie23;
    typename HandleTraits<Handle>::MatrixTransparentHandleMap tie22;
    typename HandleTraits<Handle>::ReferenceToDouble tie33;
    typename HandleTraits<Handle>::ReferenceToUInt64 num_censored;
};

// ----------------------------------------------------------------------

AnyType rb_coxph_step_transition::run(AnyType& args)
{
    RBCoxPHTransitionState<MutableArrayHandle<double> > state = args[0];
    if (args[1].isNull() || args[2].isNull()) { return args[0]; }
    MappedColumnVector x;
    try {
        // an exception is raised in the backend if input data contains nulls
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        // independent variable array contains NULL. We skip this row
        return args[0];
    }
    double y = args[2].getAs<double>();
    bool status;
    if (args[3].isNull()) {
        // by default we assume that the data is uncensored => status = TRUE
        status = true;
    } else {
        status = args[3].getAs<bool>();
    }

    MappedColumnVector H = args[6].getAs<MappedColumnVector>();
    double S = args[7].getAs<double>();

    // The following check was added with MADLIB-138.
    if (!dbal::eigen_integration::isfinite(x))
        throw std::domain_error("Design matrix is not finite.");

    if (x.size() > std::numeric_limits<uint16_t>::max())
        throw std::domain_error(
            "Number of independent variables cannot be larger than 65535.");

    if (state.numRows == 0) {
        state.initialize(*this, static_cast<uint16_t>(x.size()),
                         args[4].getAs<MappedColumnVector>().data(),
                         args[5].getAs<MappedMatrix>().data());
    }

    state.numRows++;

    /** In case of a tied time of death or in the first iteration:
        We must only perform the "pre compuations". When the tie is resolved
        we add up all the precomputations once in for all. This is
        an implementation of Breslow's method.
        The time of death for two records are considered "equal" if they
        differ by less than 1.0e-6.
        Also, in case status = 0, the observation must be censored so no
        computations are required
    */

    if (std::abs(y-state.y_previous) > 1.0e-6 && state.numRows != 1) {
        state.M = state.M - (state.tie12 + trans(state.tie12)) * state.A
            + (state.tie13 * trans(state.B) + state.B * trans(state.tie13))
            - (state.tie23 * trans(state.B) + state.B * trans(state.tie23)) * state.A
            + state.tie22 * state.A * state.A + state.tie33 * state.B * trans(state.B);
        state.tie12.setZero();
        state.tie13.setZero();
        state.tie23.setZero();
        state.tie22.setZero();
        state.tie33 = 0;
    }

    MutableNativeColumnVector x_exp_coef_x(allocateArray<double>(x.size()));
    double exp_coef_x = std::exp(trans(state.coef)*x);
    x_exp_coef_x = exp_coef_x * x;
    MutableNativeColumnVector x_hs(allocateArray<double>(x.size()));
    x_hs = x - H/S;
    state.y_previous = y;
    if (status == 1) {
        state.A += 1. / S;
        state.B += H / (S*S);
        state.M += x_hs * trans(x_hs);
        state.tie12 += x_hs * trans(x_exp_coef_x);
        state.tie13 += x_hs * exp_coef_x;
        // state.num_censored++;
    }
    state.tie23 += x_exp_coef_x * exp_coef_x;
    state.tie22 += x_exp_coef_x * trans(x_exp_coef_x);
    state.tie33 += exp_coef_x * exp_coef_x;

    return state;
}

// ----------------------------------------------------------------------

AnyType rb_coxph_step_final::run(AnyType& args)
{
    RBCoxPHTransitionState<MutableArrayHandle<double> > state = args[0];
    if (state.numRows == 0) return Null();

    // For the last tie
    state.M = state.M - (state.tie12 + trans(state.tie12)) * state.A
        + (state.tie13 * trans(state.B) + state.B * trans(state.tie13))
        - (state.tie23 * trans(state.B) + state.B * trans(state.tie23)) * state.A
        + state.tie22 * state.A * state.A + state.tie33 * state.B * trans(state.B);

    if (!state.M.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
                                       "calulation. Input data is likely of poor numerical condition.");

    // Computing pseudo inverse of a PSD matrix
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.hessian, EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_hessian = decomposition.pseudoInverse();

    Matrix sandwich = inverse_of_hessian * state.M * inverse_of_hessian;
    ColumnVector sig = sandwich.diagonal();

    MutableNativeColumnVector std_err(
        this->allocateArray<double>(state.coef.size()));
    MutableNativeColumnVector waldZStats(
        this->allocateArray<double>(state.coef.size()));
    MutableNativeColumnVector waldPValues(
        this->allocateArray<double>(state.coef.size()));
    for (int i = 0; i < sig.size(); i++) {
        std_err(i) = sqrt(sig(i));
        waldZStats(i) = state.coef(i) / std_err(i);
        waldPValues(i) = 2. * prob::cdf( prob::normal(),
                                         -std::abs(waldZStats(i)));
    }

    AnyType tuple;
    tuple << std_err << waldZStats << waldPValues;
    return tuple;
}

// ----------------------------------------------------------------------
// ----------------------------------------------------------------------
// ----------------------------------------------------------------------

// The window function

/**
 * @brief Transition state for the CoxPH robust variance for computing H
 * and S in the window function
 */
template <class Handle>
class RBHSTransitionState {

    template <class OtherHandle>
    friend class RBHSTransitionState;

  public:
    RBHSTransitionState(const AnyType &inArray)
        : mStorage(inArray.getAs<Handle>()) {

        rebind(static_cast<uint16_t>(mStorage[1]));
    }

    /**
     * @brief Convert to backend representation
     *
     * We define this function so that we can use TransitionState in the argument
     * list and as a return type.   */
    inline operator AnyType() const {
        return mStorage;
    }

    /**
     * @brief Initialize the transition state. Only called for first row.
     *
     * @param inAllocator Allocator for the memory transition state. Must fill
     *     the memory block with zeros.
     * @param inWidthOfX Number of independent variables. The first row of data
     *     determines the size of the transition state. This size is a quadratic
     *     function of inWidthOfX.
     */
    inline void initialize(const Allocator &inAllocator, uint16_t inWidthOfX,
                           const double * inCoef = 0)
    {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                                             dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(inWidthOfX));
        rebind(inWidthOfX);
        widthOfX = inWidthOfX;

        if(inCoef){
            for(uint16_t i = 0; i < widthOfX; i++)
                coef[i] = inCoef[i];
        }
        this->reset();
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        S = 0;
        H.fill(0);
    }

  private:
    static inline size_t arraySize(const uint16_t inWidthOfX) {
        return 3 + 2 * inWidthOfX;
    }

    void rebind(uint16_t inWidthOfX) {
        numRows.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        coef.rebind(&mStorage[2], inWidthOfX);
        S.rebind(&mStorage[2+inWidthOfX]);
        H.rebind(&mStorage[3+inWidthOfX], inWidthOfX);
    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap coef;
    typename HandleTraits<Handle>::ReferenceToDouble S;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap H;
};


// ----------------------------------------------------------------------

AnyType coxph_h_s_transition::run(AnyType& args)
{
    RBHSTransitionState<MutableArrayHandle<double> > state = args[0];
    if (args[1].isNull()) return args[0];

    MappedColumnVector x;
    try {
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        // independent variable array contains NULL. We skip this row
        return args[0];
    }

    // The following check was added with MADLIB-138.
    if (!dbal::eigen_integration::isfinite(x))
        throw std::domain_error("Design matrix is not finite.");

    if (x.size() > std::numeric_limits<uint16_t>::max())
        throw std::domain_error(
            "Number of independent variables cannot be larger than 65535.");

    if (state.numRows == 0) {
        state.initialize(*this, static_cast<uint16_t>(x.size()),
                         args[2].getAs<MappedColumnVector>().data());
    }

    state.numRows++;

    MutableNativeColumnVector x_exp_coef_x(allocateArray<double>(x.size()));
    double exp_coef_x = std::exp(trans(state.coef)*x);
    x_exp_coef_x = exp_coef_x * x;

    state.S += exp_coef_x;
    state.H += x_exp_coef_x;

    return state;
}

// -------------------------------------------------------------------------

AnyType coxph_h_s_merge::run(AnyType &/* args */)
{
    throw std::logic_error("The aggregate is used as an aggregate over window. "
                           "The merge function should not be used in this scenario.");
}

// ----------------------------------------------------------------------

AnyType coxph_h_s_final::run(AnyType& args)
{
    RBHSTransitionState<MutableArrayHandle<double> > state = args[0];
    if (state.numRows == 0) return Null();

    MutableNativeColumnVector H(
        this->allocateArray<double>(state.coef.size()));
    for (int i = 0; i < state.coef.size(); i++) H(i) = state.H(i);
    AnyType tuple;
    tuple << H << static_cast<double>(state.S);

    return tuple;
}

// ----------------------------------------------------------------------

AnyType rb_coxph_strata_step_final::run(AnyType& args)
{
    RBCoxPHTransitionState<MutableArrayHandle<double> > state = args[0];
    if (state.numRows == 0) return Null();

    if (!state.M.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
                                       "calulation. Input data is likely of poor numerical condition.");

    // For the last tie
    state.M = state.M - (state.tie12 + trans(state.tie12)) * state.A
        + (state.tie13 * trans(state.B) + state.B * trans(state.tie13))
        - (state.tie23 * trans(state.B) + state.B * trans(state.tie23)) * state.A
        + state.tie22 * state.A * state.A + state.tie33 * state.B * trans(state.B);

    return state;
}

// ----------------------------------------------------------------------

AnyType rb_sum_strata_transition::run(AnyType& args)
{
    if (args[0].isNull()) return args[1];
    if (args[1].isNull()) return args[0];
    RBCoxPHTransitionState<MutableArrayHandle<double> > state = args[0];
    RBCoxPHTransitionState<ArrayHandle<double> > in_state = args[1];

    state.M += in_state.M;

    return state;
}

// ----------------------------------------------------------------------

AnyType rb_sum_strata_final::run(AnyType& args)
{
    RBCoxPHTransitionState<ArrayHandle<double> > state = args[0];
    if (state.numRows == 0) return Null();

    if (!state.M.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
                                       "calulation. Input data is likely of poor numerical condition.");

    // Computing pseudo inverse of a PSD matrix
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.hessian, EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_hessian = decomposition.pseudoInverse();

    Matrix sandwich = inverse_of_hessian * state.M * inverse_of_hessian;
    ColumnVector sig = sandwich.diagonal();

    MutableNativeColumnVector std_err(
        this->allocateArray<double>(state.coef.size()));
    MutableNativeColumnVector waldZStats(
        this->allocateArray<double>(state.coef.size()));
    MutableNativeColumnVector waldPValues(
        this->allocateArray<double>(state.coef.size()));
    for (int i = 0; i < sig.size(); i++) {
        std_err(i) = sqrt(sig(i));
        waldZStats(i) = state.coef(i) / std_err(i);
        waldPValues(i) = 2. * prob::cdf( prob::normal(),
                                         -std::abs(waldZStats(i)));
    }

    AnyType tuple;
    tuple << std_err << waldZStats << waldPValues;
    return tuple;
}

}
}
}
