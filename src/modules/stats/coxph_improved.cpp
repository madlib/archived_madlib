/* ----------------------------------------------------------- */
/**
 *
 * @file cox_proportional_hazards.cpp
 *
 * @brief Cox proportional hazards
 *
 * ----------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>
#include <math.h>

#include "coxph_improved.hpp"
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

/**
 * @brief Compute the diagnostic statistics
 *
 */
AnyType stateToResult(
    const Allocator &inAllocator, const ColumnVector &inCoef,
    const ColumnVector &diagonal_of_inverse_of_hessian,
    double logLikelihood, const Matrix &inHessian,
    int nIter, const ColumnVector &stds)
{
   MutableNativeColumnVector coef(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector std_err(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector waldZStats(
        inAllocator.allocateArray<double>(inCoef.size()));
    MutableNativeColumnVector waldPValues(
        inAllocator.allocateArray<double>(inCoef.size()));

    for (Index i = 0; i < inCoef.size(); ++i) {
        coef(i) = inCoef(i) / stds(i);
        std_err(i) = std::sqrt(diagonal_of_inverse_of_hessian(i)) / stds(i);
        waldZStats(i) = coef(i) / std_err(i);
        waldPValues(i) = 2. * prob::cdf( prob::normal(),
                                         -std::abs(waldZStats(i)));
    }

    // Hessian being symmetric is updated as lower triangular matrix.
    // We need to convert diagonal matrix to full-matrix before output
    Matrix full_hessian = inHessian + inHessian.transpose();
    full_hessian.diagonal() /= 2;
    for (Index i = 0; i < inCoef.size(); ++i)
        for (Index j = 0; j < inCoef.size(); ++j)
            full_hessian(i,j) *= stds(i) * stds(j);

    // Return all coefficients, standard errors, etc. in a tuple
    AnyType tuple;
    tuple << coef << logLikelihood << std_err << waldZStats << waldPValues
          << full_hessian << nIter;
    return tuple;
}

// ------------------------------------------------------------

AnyType split_transition::run(AnyType &args)
{
    double val = args[1].getAs<double>();
    int buff_size = args[2].getAs<int>();
    int num_splits = args[3].getAs<int>();
    if (num_splits == 1)
        return Null();

    MutableArrayHandle<double> state(NULL);
    if (args[0].isNull()) {
        state = allocateArray<double>(buff_size + 2);
        state[0] = 0;
        state[1] = num_splits;
    } else {
        state = args[0].getAs<MutableArrayHandle<double> >();
    }

    // The pre-allocated buffer has been filled up
    if (state[0] >= buff_size) {
        return state;
    }

    state[static_cast<int>(++state[0]) + 1] = val;
    return state;
}

// ------------------------------------------------------------

AnyType split_merge::run(AnyType &args)
{
    if (args[0].isNull())
        return args[1];
    if (args[1].isNull())
        return args[0];

    ArrayHandle<double> state1 = args[0].getAs<ArrayHandle<double> >();
    ArrayHandle<double> state2 = args[1].getAs<ArrayHandle<double> >();

    int merged_size = static_cast<int>(state1[0]) + static_cast<int>(state2[0]);
    MutableArrayHandle<double> merged_state = allocateArray<double>(merged_size + 2);
    merged_state[0] = merged_size;
    merged_state[1] = state1[1];

    memcpy(merged_state.ptr() + 2, state1.ptr() + 2,
            static_cast<int>(state1[0]) * sizeof(double));

    memcpy(merged_state.ptr() + 2 + static_cast<int>(state1[0]),
            state2.ptr() + 2, static_cast<int>(state2[0]) * sizeof(double));

    return merged_state;
}

// ------------------------------------------------------------

AnyType split_final::run(AnyType &args)
{
    if (args[0].isNull())
        return args[0];

    MutableArrayHandle<double> state = args[0].getAs<MutableArrayHandle<double> >();

    int num_splits = static_cast<int>(state[1]);
    if (num_splits == 1) {
        return Null();
    }
    int size_splits = static_cast<int>(state[0]) / num_splits;

    if (num_splits > state[0])
        throw std::runtime_error("The number of splits is too large.");

    // Sort the sample
    std::sort(state.ptr() + 2, state.ptr() + state.size());

    MutableArrayHandle<double> splits = allocateArray<double>(num_splits - 1);
    for (int i = 0; i < num_splits - 1; i++)
        splits[i] = state[size_splits * (i + 1) - 1 + 2];

    return splits;
}

// ------------------------------------------------------------

AnyType compute_grpid::run(AnyType &args)
{
    // When no split points provided, simply return 0 (single group)
    // This can be used to deal with the very small strata case
    if (args[0].isNull())
        return 0;
    MappedColumnVector splits = args[0].getAs<MappedColumnVector>();
    double val = args[1].getAs<double>();
    // Decreasing group ids from left to right
    bool inverse = args[2].getAs<bool>();

    std::vector<double> v(splits.data(), splits.data() + splits.size());

    if (inverse)
        return static_cast<int>(v.end() - std::lower_bound(v.begin(), v.end(), val));
    else
        return static_cast<int>(std::lower_bound(v.begin(), v.end(), val) - v.begin());
}

// ------------------------------------------------------------

AnyType compute_coxph_result::run(AnyType &args) {
    MappedColumnVector coef = args[0].getAs<MappedColumnVector>();
    double L = args[1].getAs<double>();
    MappedColumnVector d2L = args[2].getAs<MappedColumnVector>();
    int nIter = args[3].getAs<int>();
    MappedColumnVector stds = args[4].getAs<MappedColumnVector>();

    int m = static_cast<int>(coef.size());
    Matrix hessian = d2L;
    hessian.resize(m, m);

    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        hessian, EigenvaluesOnly, ComputePseudoInverse);

    return stateToResult(*this, coef,
                         decomposition.pseudoInverse().diagonal(),
                         L, hessian, nIter, stds);
}

// ------------------------------------------------------------

// The transition function
// No need to deal with NULL, because all NULL values have been
// filtered out during creating the re-distributed table.
AnyType coxph_improved_step_transition::run(AnyType& args)
{
    // Current state, independant variables & dependant variables
    CoxPHState<MutableArrayHandle<double> > state = args[0];
    MappedColumnVector y = args[2].getAs<MappedColumnVector>();
    // status is converted to int in the python code
    ArrayHandle<int32_t> status = args[3].getAs<ArrayHandle<int32_t> >();
    MappedColumnVector coef = args[4].getAs<MappedColumnVector>();
    MappedColumnVector max_coef = args[5].getAs<MappedColumnVector>();
    MappedMatrix xx = args[1].getAs<MappedMatrix>();

    // The following check was added with MADLIB-138.
    if (!dbal::eigen_integration::isfinite(xx))
        throw std::domain_error("Design matrix is not finite.");

    if (state.numRows == 0) {
        state.initialize(*this, static_cast<uint16_t>(coef.size()), coef.data());
        for (size_t i = 0; i < state.widthOfX; i++) state.max_coef(i) = max_coef(i);
    }

    for (int i = 0; i < xx.cols(); i++)
    {
        const ColumnVector& x = xx.col(i);
        double exp_coef_x = std::exp(trans(coef) * x);

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

        if (std::abs(y[i] - state.y_previous) < 1.0e-6 || state.numRows == 1) {
            if (status[i] == 1) {
                state.multiplier++;
            }
        }
        else {
            state.grad -= state.multiplier*state.H/state.S;
            triangularView<Lower>(state.hessian) -=
                ((state.H*trans(state.H))/(state.S*state.S)
                 - state.V/state.S)*state.multiplier;
            state.logLikelihood -=  state.multiplier*std::log(state.S);
            state.multiplier = status[i];

        }

        /** These computations must always be performed irrespective of whether
            there are ties or not.
            Note: See design documentation for details on the implementation.
        */
        state.S += exp_coef_x;
        state.H += exp_coef_x * x;
        state.V += x * trans(x) * exp_coef_x;
        state.y_previous = y[i];
        if (status[i] == 1) {
            state.tdeath += 1;
            state.grad += x;
            state.logLikelihood += std::log(exp_coef_x);
        }
    }
    return state;
}

// ------------------------------------------------------------

/**
 * @brief Newton method final step for Cox Proportional Hazards
 */
// This is the same as the old implementation.
AnyType coxph_improved_step_final::run(AnyType& args)
{
    CoxPHState<MutableArrayHandle<double> > state = args[0];

    // If we haven't seen any data, just return Null.
    if (state.numRows == 0)
        return Null();

    if (!state.hessian.is_finite() || !state.grad.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
                                       "calulation. Input data is likely of poor numerical condition.");

   // First merge all tied times of death for the last row
    state.grad -= state.multiplier*state.H/state.S;
    triangularView<Lower>(state.hessian) -= ((state.H*trans(state.H))/(state.S*state.S)
         - state.V/state.S)*state.multiplier;
    state.logLikelihood -= state.multiplier*std::log(state.S);

    if (isinf(static_cast<double>(state.logLikelihood)) || isnan(static_cast<double>(state.logLikelihood)))
        throw NoSolutionFoundException("Over- or underflow in intermediate "
                                       "calulation. Input data is likely of poor numerical condition.");

    // Computing pseudo inverse of a PSD matrix
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.hessian, EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_hessian = decomposition.pseudoInverse();

    // Newton step
    //state.coef += state.hessian.inverse()*state.grad;
    state.coef += inverse_of_hessian * state.grad;

    // Limit the values of coef if necessary
    if (state.max_coef(0) > -1) { // iterations use max_coef
        for (size_t i = 0; i < state.widthOfX; i++)
            if (state.coef(i) > state.max_coef(i))
                state.coef(i) = state.max_coef(i);
            else if (state.coef(i) < - state.max_coef(i))
                state.coef(i) = - state.max_coef(i);
    } else {
        // first iteration computes max_coef
        for (size_t i = 0; i < state.widthOfX; i++)
            state.max_coef(i) = 20 * sqrt(state.hessian(i,i) / state.tdeath);
    }

    // Return all coefficients etc. in a tuple
    AnyType tuple;
    tuple << state.coef
        << static_cast<double>(state.logLikelihood)
        << MappedColumnVector(state.hessian.data(), state.hessian.rows() * state.hessian.cols()) // Python doesn't support 2d array
        << state.max_coef;
    return tuple;
}

// ------------------------------------------------------------

AnyType coxph_improved_strata_step_final::run(AnyType& args)
{
    CoxPHState<MutableArrayHandle<double> > state = args[0];

    // If we haven't seen any data, just return Null.
    if (state.numRows == 0)
        return Null();

    if (!state.hessian.is_finite() || !state.grad.is_finite())
        throw NoSolutionFoundException("Over- or underflow in intermediate "
                                       "calulation. Input data is likely of poor numerical condition.");

    // Computing pseudo inverse of a PSD matrix
    SymmetricPositiveDefiniteEigenDecomposition<Matrix> decomposition(
        state.hessian, EigenvaluesOnly, ComputePseudoInverse);
    Matrix inverse_of_hessian = decomposition.pseudoInverse();

    // Newton step
    //state.coef += state.hessian.inverse()*state.grad;
    state.coef += inverse_of_hessian * state.grad;

    // Limit the values of coef if necessary
    if (state.max_coef(0) > 0) { // iterations use max_coef
        for (size_t i = 0; i < state.widthOfX; i++)
            if (state.coef(i) > state.max_coef(i))
                state.coef(i) = state.max_coef(i);
            else if (state.coef(i) < - state.max_coef(i))
                state.coef(i) = - state.max_coef(i);
    } else {
        // first iteration computes max_coef
        for (size_t i = 0; i < state.widthOfX; i++)
            state.max_coef(i) = 20 * sqrt(state.hessian(i,i) / state.tdeath);
    }

    // Return all coefficients etc. in a tuple
    AnyType tuple;
    tuple << state.coef
        << static_cast<double>(state.logLikelihood)
        << MappedColumnVector(state.hessian.data(), state.hessian.rows() * state.hessian.cols()) // Python doesn't support 2d array
        << state.max_coef;
    return tuple;
}

// ------------------------------------------------------------

AnyType array_avg_transition::run(AnyType& args)
{
    if (args[1].isNull()) { return args[0]; }
    MappedColumnVector x;
    try {
        MappedColumnVector xx = args[1].getAs<MappedColumnVector>();
        x.rebind(xx.memoryHandle(), xx.size());
    } catch (const ArrayWithNullException &e) {
        return args[0];
    }
    bool use_abs = args[2].getAs<bool>();
    MutableArrayHandle<double> state = NULL;
    if(args[0].isNull())
        state = allocateArray<double, dbal::AggregateContext, dbal::DoZero, dbal::ThrowBadAlloc>(x.size() + 1);
    else
        state = args[0].getAs<MutableArrayHandle<double> >();

    state[0] += 1;
    if (use_abs)
        for (int i = 1; i <= x.size(); i++)
            state[i] += fabs(x(i-1));
    else
        for (int i = 1; i <= x.size(); i++)
            state[i] += x(i-1);

    return state;
}

// ------------------------------------------------------------

AnyType array_avg_merge::run(AnyType& args)
{
    if (args[0].isNull())
        return args[1];
    if (args[1].isNull())
        return args[0];

    MutableArrayHandle<double> left = args[0].getAs<MutableArrayHandle<double> >();
    ArrayHandle<double> right = args[1].getAs<ArrayHandle<double> >();
    for (size_t i = 0; i < left.size(); i++)
        left[i] += right[i];

    return left;
}

// ------------------------------------------------------------

AnyType array_avg_final::run(AnyType& args)
{
    if(args[0].isNull())
        return args[0];
    ArrayHandle<double> state = args[0].getAs<ArrayHandle<double> >();
    MutableArrayHandle<double> avg = allocateArray<double>(state.size() - 1);
    for (size_t i = 0; i < avg.size(); i++)
        avg[i] = state[i+1] / state[0];
    return avg;
}

// ------------------------------------------------------------

AnyType array_element_min::run(AnyType &args) {
    if(args[0].isNull())
        return args[1];
    if(args[1].isNull())
        return args[0];

    MutableMappedColumnVector state = args[0].getAs<MutableMappedColumnVector>();
    MappedColumnVector array = args[1].getAs<MappedColumnVector>();

    if(state.size() != array.size())
        throw std::runtime_error("The dimension mismatch.");

    for(int i = 0; i < state.size(); i++)
        state(i) = min(state(i), array(i));

    return state;
}

// ------------------------------------------------------------

AnyType array_element_max::run(AnyType &args) {
    if(args[0].isNull())
        return args[1];
    if(args[1].isNull())
        return args[0];

    MutableMappedColumnVector state = args[0].getAs<MutableMappedColumnVector>();
    MappedColumnVector array = args[1].getAs<MappedColumnVector>();

    if(state.size() != array.size())
        throw std::runtime_error("The dimension mismatch.");

    for(int i = 0; i < state.size(); i++)
        state(i) = max(state(i), array(i));

    return state;
}

} // namespace stats
} // namespace modules
} // namespace madlib
