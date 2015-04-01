/* ----------------------------------------------------------------------- */
/**
 *
 * @file cox_proportional_hazards.cpp
 *
 * @brief Cox proportional hazards
 *
 * ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>
#include <math.h>

#include "cox_prop_hazards.hpp"
#include "CoxPHState.hpp"

namespace madlib {
namespace modules {
namespace stats {

// Use Eigen
using namespace dbal::eigen_integration;
// Import names from other MADlib modules
using dbal::NoSolutionFoundException;

using namespace std;

// ----------------------------------------------------------------------

AnyType coxph_step_outer_transition::run(AnyType &args) {
    if (args[0].isNull()) return args[1];
    if (args[1].isNull()) return args[0];

    CoxPHState<MutableArrayHandle<double> > stateLeft = args[0];
    CoxPHState<ArrayHandle<double> > stateRight = args[1];

    stateLeft += stateRight;
    return stateLeft;
}

// ----------------------------------------------------------------------

/**
 * @brief Newton method final step for Cox Proportional Hazards
 *
 */
AnyType coxph_step_inner_final::run(AnyType &args) {
    CoxPHState<MutableArrayHandle<double> > state = args[0];

    // If we haven't seen any data, just return Null.
    if (state.numRows == 0)
        return Null();

    if (!state.hessian.is_finite() || !state.grad.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
                                       "calulation. Input data is likely of poor numerical condition.");

    // First merge all tied times of death for the last row
    state.grad -= state.multiplier*state.H/state.S;
    triangularView<Lower>(state.hessian) -=
        ((state.H*trans(state.H))/(state.S*state.S)
         - state.V/state.S)*state.multiplier;
    state.logLikelihood -=  state.multiplier*std::log(state.S);

    // Return all coefficients etc. in a tuple
    return state;
}


// -----------------------------------------------------------------------
// Schoenfeld Residual Aggregate
// -----------------------------------------------------------------------
AnyType zph_transition::run(AnyType &args){
    if (args[1].isNull()) { return args[0]; }

    MappedColumnVector x;
    try {
        // an exception is raised in the backend if args[2] contains nulls
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
     } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    if (!dbal::eigen_integration::isfinite(x))
        throw std::domain_error("Design matrix is not finite.");

    int data_dim = static_cast<int>(x.size());
    if (data_dim > std::numeric_limits<uint16_t>::max())
        throw std::domain_error(
                "Number of independent variables cannot be larger than 65535.");

    MutableArrayHandle<double> state(NULL);
    if (args[0].isNull()){
        // state[0:data_dim-1]  - x * exp(coeff . x)
        // state[data_dim]      - exp(coeff . x)
        state = allocateArray<double>(data_dim + 1);
    } else {
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    MutableNativeColumnVector coef(allocateArray<double>(data_dim));
    if (args[2].isNull()){
        for (int i=0; i < data_dim ; i++)
            coef(i) = 0;
    } else {
        coef = args[2].getAs<MappedColumnVector>();
    }

    double exp_coef_x = std::exp(trans(coef) * x);
    MutableNativeColumnVector x_exp_coef_x(allocateArray<double>(data_dim));
    x_exp_coef_x = exp_coef_x * x;

    for (int i =0; i < data_dim; i++)
        state[i] += x_exp_coef_x[i];
    state[data_dim] += exp_coef_x;

    return state;
}
// -------------------------------------------------------------------------

AnyType zph_merge::run(AnyType &/* args */){
    throw std::logic_error("The aggregate is used as an aggregate over window. "
                           "The merge function should not be used in this scenario.");
}
// -------------------------------------------------------------------------
AnyType zph_final::run(AnyType &args){
    if (args[0].isNull())
        return Null();

    MutableArrayHandle<double> state(NULL);
    state = args[0].getAs<MutableArrayHandle<double> >();
    size_t data_dim = state.size()-1;

    MutableArrayHandle<double> result = allocateArray<double>(data_dim);
    for (size_t i = 0; i < data_dim; i++)
        result[i] = state[i]/state[data_dim];
    return result;
}

// -----------------------------------------------------------------------
// Correlation aggregate between an array and a scalar
// -----------------------------------------------------------------------
/**
 * @brief Transition state for the Cox Proportional Hazards
 *
 * TransitionState encapsulates the transition state during the
 * aggregate functions. To the database, the state is exposed
 * as a single DOUBLE PRECISION array, to the C++ code it is a proper object
 * containing scalars, a vector, and a matrix.
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length at least 5, and all elements are 0.
 */
template <class Handle>
class ArrayElemCorrState {

    template <class OtherHandle>
    friend class ArrayElemCorrState;

  public:
    ArrayElemCorrState(const AnyType &inArray)
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
    inline void initialize(const Allocator &inAllocator, uint16_t inWidthOfX) {
        mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                                             dbal::DoZero,
                                             dbal::ThrowBadAlloc>(arraySize(inWidthOfX));
        rebind(inWidthOfX);
        widthOfX = inWidthOfX;
        this->reset();
    }

    /**
     * @brief We need to support assigning the previous state
     */
    template <class OtherHandle>
    ArrayElemCorrState &operator=(
        const ArrayElemCorrState<OtherHandle> &inOtherState) {
        for (size_t i = 0; i < mStorage.size(); i++)
            mStorage[i] = inOtherState.mStorage[i];
        return *this;
    }

    /**
     * @brief Merge with another State object by copying the intra-iteration
     *     fields
     */
    template <class OtherHandle>
    ArrayElemCorrState &operator+=(
        const ArrayElemCorrState<OtherHandle> &inOtherState) {

        if (mStorage.size() != inOtherState.mStorage.size() ||
            widthOfX != inOtherState.widthOfX)
            throw std::logic_error(
                "Internal error: Incompatible transition states");

        numRows += inOtherState.numRows;
        sum_y += inOtherState.sum_y;
        sum_yy += inOtherState.sum_yy;
        sum_x += inOtherState.sum_x;
        sum_xx += inOtherState.sum_xx;
        sum_xy += inOtherState.sum_xy;

        return *this;
    }

    /**
     * @brief Reset the inter-iteration fields.
     */
    inline void reset() {
        numRows = 0;
        sum_y = 0;
        sum_yy = 0;
        sum_xy.fill(0);
        sum_x.fill(0);
        sum_xx.fill(0);
    }

  private:
    static inline size_t arraySize(const uint16_t inWidthOfX) {
        return 4 + 3 * inWidthOfX;
    }

    /**
     * @brief Rebind to a new storage array
     *
     * @param inWidthOfX The number of independent variables.
     *
     * Array layout:
     * Inter iteration components (updated in the final step)
     * - 0: numRows (number of rows seen so far)
     * - 1: widthOfX (number of features)
     * - 2: coef (multipliers for each of the features)

     * Intra interation components (updated in the current interation)
     * - 2 + widthofX: S (see design document for details)
     * - 3 + widthofX: Hi[j] (see design document for details)
     * - 3 + 2*widthofX: gradCoef (coefficients of the gradient)
     * - 3 + 3*widthofX: logLikelihood
     * - 4 + 3*widthofX: V (Precomputations for the hessian)
     * - 4 + 3*widthofX + widthofX^2: hessian
     *
     */
    void rebind(uint16_t inWidthOfX) {
        // Inter iteration components
        numRows.rebind(&mStorage[0]);
        widthOfX.rebind(&mStorage[1]);
        sum_y.rebind(&mStorage[2]);
        sum_yy.rebind(&mStorage[3]);

        // Intra iteration components
        sum_xy.rebind(&mStorage[4], inWidthOfX);
        sum_x.rebind(&mStorage[4+inWidthOfX], inWidthOfX);
        sum_xx.rebind(&mStorage[4+2*inWidthOfX], inWidthOfX);
    }

    Handle mStorage;

  public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToUInt16 widthOfX;
    typename HandleTraits<Handle>::ReferenceToDouble sum_y;
    typename HandleTraits<Handle>::ReferenceToDouble sum_yy;

    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap sum_xy;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap sum_x;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap sum_xx;
};

AnyType array_elem_corr_transition::run(AnyType &args){

    ArrayElemCorrState<MutableArrayHandle<double> > state = args[0];

    if (args[1].isNull() || args[2].isNull()) { return args[0]; }

    MappedColumnVector x;
    try {
        // an exception is raised in the backend if input data contains nulls
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        // x is a const reference, we can only rebind to change its pointer
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }

    double y = args[2].getAs<double>();

    if (!dbal::eigen_integration::isfinite(x))
        throw std::domain_error("Design matrix is not finite.");

    if (x.size() > std::numeric_limits<uint16_t>::max())
        throw std::domain_error(
            "Number of variables cannot be larger than 65535.");

    if (state.numRows == 0) {
        state.initialize(*this, static_cast<uint16_t>(x.size()));
    }

    state.numRows += 1;
    state.sum_y += y;
    state.sum_yy += y*y;
    state.sum_x += x;
    state.sum_xx += x.cwiseProduct(x);  // perform element-wise multiplication
    state.sum_xy += x * y;  // perform scalar multiplication

    return state;
}

AnyType array_elem_corr_merge::run(AnyType &args) {
    ArrayElemCorrState<MutableArrayHandle<double> > stateLeft = args[0];
    ArrayElemCorrState<ArrayHandle<double> > stateRight = args[1];

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

AnyType array_elem_corr_final::run(AnyType &args) {
    ArrayElemCorrState<MutableArrayHandle<double> > state = args[0];

    // If we haven't seen any data, just return Null.
    if (state.numRows == 0)
        return Null();

    ColumnVector S_xy =
        static_cast<double>(state.numRows) * state.sum_xy - state.sum_x  * state.sum_y;
    ColumnVector S_xx =
        static_cast<double>(state.numRows) * state.sum_xx - state.sum_x.cwiseProduct(state.sum_x);
    double S_yy = static_cast<double>(state.numRows) * state.sum_yy - state.sum_y * state.sum_y;
    ColumnVector correlation = S_xy.cwiseQuotient(S_xx.cwiseSqrt() * sqrt(S_yy));

    return correlation;
}

AnyType coxph_resid_stat_transition::run(AnyType &args) {
    double w = args[1].getAs<double>();
    ArrayHandle<double> residual = args[2].getAs<ArrayHandle<double> >();
    ArrayHandle<double> hessian = args[3].getAs<ArrayHandle<double> >();
    int m = args[4].getAs<int>();

    MutableArrayHandle<double> state(NULL);
    if(args[0].isNull()){
        int n = static_cast<int>(residual.size());
        // state[0]: m
        // state[1]: n
        // state[2]: w_t * w
        // state[3:2 + n]: w_t * residual  (size = n)
        // state[n + 3:n + n * n  + 2]: hessian (size = n*n)
        state = allocateArray<double>(n * n + n + 3);
        state[0] = m;
        state[1] = n;
        for(size_t i = 0; i < hessian.size(); i++)
            state[n + 3 + i] = hessian[i];
    }else{
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    state[2] += w * w;
    for(size_t i = 0; i < residual.size(); i++)
        state[3 + i] += residual[i] * w;

    return state;
}

AnyType coxph_resid_stat_merge::run(AnyType &args) {
    if(args[0].isNull())
        return args[1];
    if(args[1].isNull())
        return args[0];

    MutableArrayHandle<double> state1 = args[0].getAs<MutableArrayHandle<double> >();
    ArrayHandle<double> state2 = args[1].getAs<ArrayHandle<double> >();
    int n = static_cast<int>(state1[1]);

    for(int i = 2; i <= n + 2; i++)
        state1[i] += state2[i];

    return state1;
}

AnyType coxph_resid_stat_final::run(AnyType &args) {
    if(args[0].isNull())
        return Null();

    using boost::math::complement;
    MutableArrayHandle<double> state = args[0].getAs<MutableArrayHandle<double> >();
    int m = static_cast<int>(state[0]);
    int n = static_cast<int>(state[1]);
    double w_trans_w = state[2];

    Eigen::Map<Matrix> w_trans_residual(state.ptr() + 3, 1, n);
    Eigen::Map<Matrix> hessian(state.ptr() + 3 + n, n, n);

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        hessian, EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_hessian = decomposition.pseudoInverse();

    ColumnVector v =  m * (w_trans_residual * inverse_of_hessian).transpose();
    ColumnVector v_v = v.cwiseProduct(v);

    ColumnVector covar_diagonal = inverse_of_hessian.diagonal();
    ColumnVector z = v_v.cwiseQuotient(covar_diagonal * m * w_trans_w);
    ColumnVector p(n);
    for(int i = 0; i < n; i++)
        p(i) = prob::cdf(complement(prob::chi_squared(static_cast<double>(1)), z(i)));

    AnyType tuple;
    tuple << z << p;
    return tuple;
}

AnyType coxph_scale_resid::run(AnyType &args) {
    int m = args[0].getAs<int>();
    MappedMatrix hessian = args[1].getAs<MappedMatrix>();
    MappedColumnVector residual = args[2].getAs<MappedColumnVector>();

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        hessian, EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_hessian = decomposition.pseudoInverse();

    ColumnVector scaled_residual = (inverse_of_hessian * Matrix(residual)) * m;
    return scaled_residual;
}

AnyType coxph_predict_resp::run(AnyType &args) {
    try {
        args[0].getAs<MappedColumnVector>();
    } catch (const ArrayWithNullException &e) {
        throw std::runtime_error(
            "coxph error: the coefficients contain NULL values");
    }
    // returns NULL if args[1] (features) contains NULL values
    try {
        args[1].getAs<MappedColumnVector>();
    } catch (const ArrayWithNullException &e) {
        return Null();
    }

    MappedColumnVector coefs = args[0].getAs<MappedColumnVector>();
    MappedColumnVector indep = args[1].getAs<MappedColumnVector>();
    MappedColumnVector mean_indep = args[2].getAs<MappedColumnVector>();
    std::string predtype = args[3].getAs<std::string>();

    if (coefs.size() != indep.size())
        throw std::runtime_error(
            "Coefficients and independent variables are of incompatible length");
    if (coefs.size() != mean_indep.size())
        throw std::runtime_error(
            "Coefficients and mean vector of independent variables are of incompatible length");

    double dot = coefs.dot(indep);
    double meandot = coefs.dot(mean_indep);
    double diff = dot - meandot;

    if (predtype.compare("linear_predictors")==0) return(diff);
    if (predtype.compare("risk")==0) return(exp(diff));

    throw std::runtime_error("Invalid prediction type!");
}

AnyType coxph_predict_terms::run(AnyType &args) {
    try {
        args[0].getAs<MappedColumnVector>();
    } catch (const ArrayWithNullException &e) {
        throw std::runtime_error(
            "coxph error: the coefficients contain NULL values");
    }
    // returns NULL if args[1] (features) contains NULL values
    try {
        args[1].getAs<MappedColumnVector>();
    } catch (const ArrayWithNullException &e) {
        return Null();
    }

    MappedColumnVector coefs = args[0].getAs<MappedColumnVector>();
    MappedColumnVector indep = args[1].getAs<MappedColumnVector>();
    MappedColumnVector mean_indep = args[2].getAs<MappedColumnVector>();

    if (coefs.size() != indep.size())
        throw std::runtime_error(
            "Coefficients and independent variables are of incompatible length");
    if (coefs.size() != mean_indep.size())
        throw std::runtime_error(
            "Coefficients and mean vector of independent variables are of incompatible length");

    return ColumnVector(coefs.cwiseProduct(indep - mean_indep));

}
} // namespace stats
} // namespace modules
} // namespace madlib
