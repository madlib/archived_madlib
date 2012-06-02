/* ----------------------------------------------------------------------- *//**
 *
 * @file wilcoxon_signed_rank_test.cpp
 *
 * @brief Wilcoxon-Signed-Rank-test functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/prob/boost.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <utils/Math.hpp>

#include "wilcoxon_signed_rank_test.hpp"

namespace madlib {

namespace modules {

namespace stats {

/**
 * @brief Transition state for Wilcoxon signed-rank functions
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 9, and all elemenets are 0.
 */
template <class Handle>
class WSRTestTransitionState {
public:
    WSRTestTransitionState(const AnyType &inArray)
      : mStorage(inArray.getAs<Handle>()),
        num(&mStorage[0], 2),
        numTies(&mStorage[2], 2),
        rankSum(&mStorage[4], 2),
        lastAbs(&mStorage[6]),
        lastAbsUpperBound(&mStorage[7]),
        reduceVariance(&mStorage[8]) { }
    
    inline operator AnyType() const {
        return mStorage;
    }
    
private:
    Handle mStorage;

public:
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap num;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap numTies;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap rankSum;
    typename HandleTraits<Handle>::ReferenceToDouble lastAbs;
    typename HandleTraits<Handle>::ReferenceToDouble lastAbsUpperBound;
    typename HandleTraits<Handle>::ReferenceToDouble reduceVariance;
};


/**
 * @brief Perform the Wilcoxon-Signed-Rank-test transition step
 *
 * Index 0 always refers to the positive values and index 1 refers to
 * the negative values.
 */
AnyType
wsr_test_transition::run(AnyType &args) {
    WSRTestTransitionState<MutableArrayHandle<double> > state = args[0];
    double value = args[1].getAs<double>();
    AnyType precisionArg = args.numFields() >= 3 ? args[2] : Null();
    double precision = precisionArg.isNull()
        ? value * std::numeric_limits<double>::epsilon()
        : precisionArg.getAs<double>();
    
    // Ignore values of zero.
    if (value == 0)
        return state;
    
    double absValue = std::fabs(value);
    int sample = value > 0 ? 0 : 1;
    
    if (state.num.sum() > 0) {
        if (absValue < state.lastAbs)
            throw std::invalid_argument("Must be used as an ordered aggregate, "
                "in ascending order of the absolute value of the first "
                "argument.");
        else if (absValue - precision <= state.lastAbsUpperBound) {
            for (int i = 0; i <= 1; i++)
                state.rankSum(i) += state.numTies(i) * 0.5;
            
            // FIXME: There are conflicting sources for the variance reduction:
            // http://mlsc.lboro.ac.uk/resources/statistics/wsrt.pdf
            // http://de.wikipedia.org/wiki/Wilcoxon-Vorzeichen-Rang-Test
            //
            // For each *group*, we want to add (t^3 - t)/48 to state.reduceVariance
            // where t denotes the number of ties in that group
            // Note that t^3 - t == 0 if t = 0 or t = 1. So it is sufficient
            // to modify state.reduceVariance only in the current case of the
            // if-else block.
            //
            // For each repeated occurrence, we add
            // [ (t+1)^3 - (t+1) - (t^3 - t) ]/48 to state.reduceVariance.
            double t = state.numTies.sum();
            state.reduceVariance += 1./16. * t * (t + 1);
        } else {
            // We have here:
            // state.lastAbs <= state.lastAbsUpperBound < absValue - precision
            //               <= absValue
            state.numTies.setZero();
        }
    }
    
    state.num(sample)++;
    state.rankSum(sample) += (2. * state.num.sum() - state.numTies.sum()) / 2.;
    state.numTies(sample)++;
    state.lastAbs = absValue;
    if (absValue + precision > state.lastAbsUpperBound)
        state.lastAbsUpperBound = absValue + precision;
    
    return state;
}

AnyType
wsr_test_final::run(AnyType &args) {
    using boost::math::complement;

    WSRTestTransitionState<ArrayHandle<double> > state = args[0];

    double n_n1 = state.num.sum() * (state.num.sum() + 1);
    double statistic = state.rankSum.minCoeff();
    double z_statistic = (state.rankSum(0) - n_n1 / 4.)
                       / std::sqrt(n_n1 * (2 * state.num.sum() + 1.) / 24.
                            - state.reduceVariance);
    
    AnyType tuple;
    tuple
        << statistic
        << static_cast<double>(state.rankSum(0))
        << static_cast<double>(state.rankSum(1))
        << static_cast<int64_t>(state.num.sum())
        << z_statistic
        << prob::cdf(complement(prob::normal(), z_statistic))
        << 2. * prob::cdf(complement(prob::normal(), std::fabs(z_statistic)));
    return tuple;
}

} // namespace stats

} // namespace modules

} // namespace madlib
