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
 * database with length 6, and all elements are 0. Handle::operator[] will
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
        df(&mStorage[5]) { }

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
    // §4.15.4 ("Aggregate functions") of ISO/IEC 9075-2:2003, "SQL/Foundation"
    // demands that rows containing NULLs are ignored
    // We currently rely on the backend filtering out rows with NULLs, i.e., to
    // perform an action equivalent to:
    // for (uint16_t i = 0; i < args.numFields(); ++i)
    //    if (args[i].isNull)
    //        return state;

    Chi2TestTransitionState<MutableArrayHandle<double> > state = args[0];
    double observed = static_cast<double>(args[1].getAs<int64_t>());
    double expected = args.numFields() <= 2 ? 1 : args[2].getAs<double>();
    int64_t df = args.numFields() <= 3 ? 0 : args[3].getAs<int64_t>();

    if (observed < 0)
        throw std::invalid_argument("Number of observations must be "
            "nonnegative.");
    else if (df < 0)
        throw std::invalid_argument("Degree of freedom must be positive (or 0 "
            "to use the default of <number of rows> - 1).");
    else if (state.df != df) {
        if (state.numRows > 0)
            throw std::invalid_argument("Degree of freedom must be constant.");
        state.df = df;
    }

    updateSumSquaredDeviations(state.numRows.ref(), state.sum_expect.ref(),
        state.sum_obs_square_over_expect.ref(), state.sum_obs.ref(),
        state.sumSquaredDeviations.ref(),
        1, expected, observed * observed / expected,
        observed, 0);

    return state;
}

AnyType
chi2_gof_test_merge_states::run(AnyType &args) {
    Chi2TestTransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    Chi2TestTransitionState<ArrayHandle<double> > stateRight = args[1];

    if (stateLeft.df != stateRight.df) {
        if (stateLeft.numRows == 0)
            stateLeft.df = stateRight.df;
        else if (stateRight.numRows > 0)
            throw std::invalid_argument("Degree of freedom must be constant.");
    }

    // Merge states together and return
    updateSumSquaredDeviations(
        stateLeft.numRows.ref(), stateLeft.sum_expect.ref(),
            stateLeft.sum_obs_square_over_expect.ref(),
            stateLeft.sum_obs.ref(), stateLeft.sumSquaredDeviations.ref(),
        stateRight.numRows.ref(), stateRight.sum_expect,
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

    int64_t degreeOfFreedom = state.df == 0 ? state.numRows - 1 : state.df;
    double statistic = state.sumSquaredDeviations / state.sum_obs;

    // Phi coefficient
    double phi = std::sqrt(statistic / static_cast<double>(state.numRows));

    // Contingency coefficient
    double C = std::sqrt(statistic
             / (static_cast<double>(state.numRows) + statistic));

    AnyType tuple;
    tuple
        << statistic
        << (degreeOfFreedom > 0
            ? prob::cdf(complement(prob::chi_squared(
                static_cast<double>(degreeOfFreedom)), statistic))
            : Null())
        << degreeOfFreedom
        << phi
        << C;
    return tuple;
}

} // namespace stats

} // namespace modules

} // namespace madlib
