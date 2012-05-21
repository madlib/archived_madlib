/* ----------------------------------------------------------------------- *//**
 *
 * @file chi_squared_test.cpp
 *
 * @brief Wilcoxon-Signed-Rank-test functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/boost.hpp>

#include "chi_squared_test.hpp"

namespace madlib {

namespace modules {

namespace stats {

/**
 * @brief Transition state for chi-squared functions
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 7, and all elements are 0. Handle::operator[] will
 * perform bounds checking.
 */
template <class Handle>
class Chi2TestTransitionState {
public:
    Chi2TestTransitionState(const AnyType &inArray)
      : mStorage(inArray.getAs<Handle>()),
        numRows(&mStorage[0]),
        sum_expect(&mStorage[1]),
        sum_obs_square_over_expect(&mStorage[2]),
        sum_obs(&mStorage[3]),
        sumSquaredDeviations(&mStorage[4]),
        uniformDist(&mStorage[5]),
        df(&mStorage[6]) { }
    
    inline operator AnyType() const {
        return mStorage;
    }
    
private:
    Handle mStorage;

public:
    typename HandleTraits<Handle>::ReferenceToUInt64 numRows;
    typename HandleTraits<Handle>::ReferenceToDouble sum_expect;
    typename HandleTraits<Handle>::ReferenceToDouble sum_obs_square_over_expect;
    typename HandleTraits<Handle>::ReferenceToDouble sum_obs;
    typename HandleTraits<Handle>::ReferenceToDouble sumSquaredDeviations;
    
    typename HandleTraits<Handle>::ReferenceToBool uniformDist;
    typename HandleTraits<Handle>::ReferenceToInt64 df;
};

inline
void
updateSumSquaredDeviations(double &ioLeftNumRows, double &ioLeftSumExp,
    double &ioLeftSumObsSquareOverExp, double &ioLeftSumObs,
    double &ioLeftSumSquaredDeviations,
    double inRightNumRows, double inRightSumExp,
    double inRightSumObsSquareOverExp, double inRightSumObs,
    double inRightSumSquaredDeviations) {
    
    if (inRightNumRows <= 0)
        return;
    
    // FIXME: Use compensated sums for numerical stability
    // http://jira.madlib.net/browse/MADLIB-501
    ioLeftSumSquaredDeviations
           += inRightSumSquaredDeviations
            + ioLeftSumExp * inRightSumObsSquareOverExp
            + ioLeftSumObsSquareOverExp * inRightSumExp
            - 2 * ioLeftSumObs * inRightSumObs;
    
    ioLeftNumRows += inRightNumRows;
    ioLeftSumExp += inRightSumExp;
    ioLeftSumObsSquareOverExp += inRightSumObsSquareOverExp;
    ioLeftSumObs += inRightSumObs;
}

AnyType
chi2_gof_test_transition::run(AnyType &args) {
    Chi2TestTransitionState<MutableArrayHandle<double> > state = args[0];
    int64_t observed = args[1].getAs<int64_t>();
    AnyType expectedArg = args.numFields() >= 3 ? args[2] : Null();
    double expected = expectedArg.isNull() ? 1 : expectedArg.getAs<double>();
    AnyType dfArg = args.numFields() >= 4 ? args[3] : Null();
    int64_t df = dfArg.isNull() ? -1 : dfArg.getAs<int64_t>();
    
    if (state.uniformDist != expectedArg.isNull()) {
        if (state.numRows > 0)
            throw std::invalid_argument("Expected number of observations must "
                "be given for all events or must be NULL for all events, in "
                "which case a discrete uniform distribution is assumed.");
        state.uniformDist = expectedArg.isNull();
    }
    
    if (!dfArg.isNull() && df <= 0)
        throw std::invalid_argument("Degree of freedom must be positive.");
    else if (state.df != df) {
        if (state.numRows > 0)
            throw std::invalid_argument("Degree of freedom must be constant.");
        state.df = df;
    }

    updateSumSquaredDeviations(state.numRows, state.sum_expect,
        state.sum_obs_square_over_expect, state.sum_obs,
        state.sumSquaredDeviations,
        1, expected, static_cast<double>(observed) * observed / expected,
        observed, 0);
    
    return state;
}

AnyType
chi2_gof_test_merge_states::run(AnyType &args) {
    Chi2TestTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    Chi2TestTransitionState<ArrayHandle<double> > stateRight = args[1];
    
    if (stateLeft.uniformDist != stateRight.uniformDist) {
        if (stateLeft.numRows == 0)
            stateLeft.uniformDist = stateRight.uniformDist;
        else if (stateRight.numRows > 0)
            throw std::invalid_argument("Expected number of observations must "
                "be given for all events or must be NULL for all events, in "
                "which case a discrete uniform distribution is assumed.");
    }
    
    if (stateLeft.df != stateRight.df) {
        if (stateLeft.numRows == 0)
            stateLeft.df = stateRight.df;
        else if (stateRight.numRows > 0)
            throw std::invalid_argument("Degree of freedom must be constant.");
    }
    
    // Merge states together and return
    updateSumSquaredDeviations(
        stateLeft.numRows, stateLeft.sum_expect,
            stateLeft.sum_obs_square_over_expect,
            stateLeft.sum_obs, stateLeft.sumSquaredDeviations,
        stateRight.numRows, stateRight.sum_expect,
            stateRight.sum_obs_square_over_expect,
            stateRight.sum_obs, stateRight.sumSquaredDeviations);
    
    return stateLeft;
}

AnyType
chi2_gof_test_final::run(AnyType &args) {
    using boost::math::complement;

    Chi2TestTransitionState<ArrayHandle<double> > state = args[0];

    // If we haven't seen any data, just return Null. This is the standard
    // behavior of aggregate function on empty data sets (compare, e.g.,
    // how PostgreSQL handles sum or avg on empty inputs)
    if (state.numRows == 0)
        return Null();
    
    int64_t degreeOfFreedom = state.df < 0 ? state.numRows - 1 : state.df;
    double statistic = state.sumSquaredDeviations / state.sum_obs;
    
    // Phi coefficient
    double phi = std::sqrt(statistic / state.numRows);
    
    // Contingency coefficient
    double C = std::sqrt(statistic / (state.numRows + statistic));
    
    AnyType tuple;
    tuple
        << statistic
        << prob::cdf(complement(prob::chi_squared(degreeOfFreedom), statistic))
        << degreeOfFreedom
        << phi
        << C;
    return tuple;
}

} // namespace stats

} // namespace modules

} // namespace madlib
