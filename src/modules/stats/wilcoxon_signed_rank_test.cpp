/* ----------------------------------------------------------------------- *//**
 *
 * @file wilcoxon_signed_rank_test.cpp
 *
 * @brief Wilcoxon-Signed-Rank-test functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/prob.hpp>

// We use string concatenation with the + operator
#include <string>

namespace madlib {

namespace modules {

// Import names from other MADlib modules
using prob::normalCDF;

namespace stats {

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "wilcoxon_signed_rank_test.hpp"

/**
 * @brief Transition state for Wilcoxon signed-rank functions
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 8, and all elemenets are 0.
 */
template <class Handle>
class WSRTestTransitionState : public AbstractionLayer {
public:
    WSRTestTransitionState(const AnyType &inArray)
      : mStorage(inArray.getAs<Handle>()),
        num(&mStorage[0], 2),
        numTies(&mStorage[2], 2),
        rankSum(&mStorage[4], 2),
        last(&mStorage[6]),
        reduceVariance(&mStorage[7]) { }
    
    inline operator AnyType() const {
        return mStorage;
    }
    
private:
    Handle mStorage;

public:
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap num;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap numTies;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap rankSum;
    typename HandleTraits<Handle>::ReferenceToDouble last;
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
    
    // Ignore values of zero.
    if (value == 0)
        return state;
    
    int sample = value > 0 ? 0 : 1;
    
    // FIXME: The following epsilon is hard-coded
    const double epsilon = 1e-10;
    
    if (std::fabs(state.last) + epsilon < std::fabs(value)) {
        state.numTies.setZero();
    } else if (std::fabs(std::fabs(state.last) - std::fabs(value)) < epsilon) {
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
    } else if (state.num.sum() > 0) { // also satisfied here: state.last > value
        throw std::invalid_argument("Must be used as an ordered aggregate.");
    }
    
    state.num(sample)++;
    state.rankSum(sample) += (2. * state.num.sum() - state.numTies.sum()) / 2.;
    state.numTies(sample)++;
    state.last = value;
    
    return state;
}

AnyType
wsr_test_final::run(AnyType &args) {
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
        << 1. - normalCDF(z_statistic)
        << 2. * (1. - normalCDF(std::fabs(z_statistic)));
    return tuple;
}

} // namespace stats

} // namespace modules

} // namespace madlib
