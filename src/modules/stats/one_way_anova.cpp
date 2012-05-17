/* ----------------------------------------------------------------------- *//**
 *
 * @file one_way_anova.cpp
 *
 * @brief One-way ANOVA functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/prob.hpp>
#include <utils/Math.hpp>

// We use string concatenation with the + operator
#include <string>

namespace madlib {

namespace modules {

// Import names from other MADlib modules
using prob::fisherF_CDF;

namespace stats {

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "one_way_anova.hpp"

/**
 * @brief Transition state for one-way ANOVA functions
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 2, and all elemenets are 0. Handle::operator[] will
 * perform bounds checking.
 */
template <class Handle>
class OWATransitionState : public AbstractionLayer {
public:
    OWATransitionState(const AnyType &inArray)
      : mStorage(inArray.getAs<Handle>()) {
      
        rebind(utils::nextPowerOfTwo(static_cast<uint16_t>(mStorage[0])));
    }
    
    /**
     * @brief Convert to backend representation
     *
     * We define this function so that we can use TransitionState in the argument
     * list and as a return type.
     */
    inline operator AnyType() const {
        return mStorage;
    }
    
    /**
     * @brief Return the index (in the num, sum, and corrected_square_sum
     *     fields) of a group value
     *
     * If a value is not found, we add a new group to the transition state.
     * Since we do not want to reallocate too often, we reserve some buffer
     * space in the storage array. So we need to reallocate and copy memory only
     * whenever the number of groups hits a power of 2.
     */
    uint32_t idxOfGroup(const Allocator& inAllocator, uint16_t inValue);
    
private:
    static inline size_t arraySize(uint16_t inNumGroupsReserved) {
        return 1 + 5 * inNumGroupsReserved;
    }

    void rebind(uint16_t inNumGroupsReserved) {
        madlib_assert(mStorage.size() >= arraySize(inNumGroupsReserved),
            std::runtime_error("Out-of-bounds array access detected."));
        
        numGroups.rebind(&mStorage[0]);
        groupValues = &mStorage[1];
        posToIndices = &mStorage[1 + inNumGroupsReserved];
        num.rebind(&mStorage[1 + 2 * inNumGroupsReserved], inNumGroupsReserved);
        sum.rebind(&mStorage[1 + 3 * inNumGroupsReserved], inNumGroupsReserved);
        corrected_square_sum.rebind(
            &mStorage[1 + 4 * inNumGroupsReserved], inNumGroupsReserved);
    }

    Handle mStorage;
    
public:
    typename HandleTraits<Handle>::ReferenceToUInt16 numGroups;
    typename HandleTraits<Handle>::DoublePtr groupValues;
    typename HandleTraits<Handle>::DoublePtr posToIndices;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap num;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap sum;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap corrected_square_sum;
};

template <>
uint32_t
OWATransitionState<AbstractionLayer::ArrayHandle<double> >::idxOfGroup(
    const Allocator&, uint16_t inValue) {
    
    uint16_t pos = std::lower_bound(
        groupValues, groupValues + numGroups, inValue) - groupValues;

    if (pos >= numGroups || groupValues[pos] != inValue) {
        // Did not find this group value. We have to start a new group.
        throw std::runtime_error("Could not find a grouping value during "
            "one-way ANOVA.");
    }
    return pos;
}

template <>
uint32_t
OWATransitionState<AbstractionLayer::MutableArrayHandle<double> >::idxOfGroup(
    const Allocator& inAllocator, uint16_t inValue) {
    
    // FIXME: Think of using proper iterators. Add some safety. Overflow checks.
    // http://jira.madlib.net/browse/MADLIB-499
    uint16_t pos = std::lower_bound(
        groupValues, groupValues + numGroups, inValue) - groupValues;
    
    if (pos >= numGroups || groupValues[pos] != inValue) {
        // Did not find this group value. We have to start a new group.
        
        uint16_t numGroupsReserved = utils::nextPowerOfTwo(
            static_cast<uint16_t>(numGroups));
        if (numGroupsReserved > numGroups) {
            // We have enough reserve space allocated.
            std::copy(groupValues + pos, groupValues + numGroups,
                groupValues + pos + 1);
            groupValues[pos] = inValue;
            
            std::copy(posToIndices + pos, posToIndices + numGroups,
                posToIndices + pos + 1);
            posToIndices[pos] = numGroups++;
        } else {
            // We need to reallocate storage for the transition state
            // Save our current state, so we can subsequently restore it
            // with the new storage
            OWATransitionState oldSelf = *this;
            if (numGroupsReserved == 0)
                numGroupsReserved = 1;
            else
                numGroupsReserved *= 2;
            mStorage = inAllocator.allocateArray<double, dbal::AggregateContext,
                dbal::DoZero, dbal::ThrowBadAlloc>(arraySize(numGroupsReserved));
            rebind(numGroupsReserved);
            
            numGroups = oldSelf.numGroups + 1;
            std::copy(oldSelf.groupValues, oldSelf.groupValues + pos, groupValues);
            std::copy(oldSelf.groupValues + pos,
                oldSelf.groupValues + oldSelf.numGroups, groupValues + pos + 1);
            groupValues[pos] = inValue;
            
            std::copy(oldSelf.posToIndices, oldSelf.posToIndices + pos, posToIndices);
            std::copy(oldSelf.posToIndices + pos,
                oldSelf.posToIndices + oldSelf.numGroups, posToIndices + pos + 1);
            posToIndices[pos] = oldSelf.numGroups;
            
            num.segment(0, oldSelf.numGroups) << oldSelf.num;
            sum.segment(0, oldSelf.numGroups) << oldSelf.sum;
            corrected_square_sum.segment(0, oldSelf.numGroups) << oldSelf.corrected_square_sum;
        }
    }
    return posToIndices[pos];
}

// FIXME: Same function used for t_test. Factor out.
// http://jira.madlib.net/browse/MADLIB-500
/**
 * @brief Update the corrected sum of squares
 *
 * For numerical stability, we should not compute the sample variance in the
 * naive way. The literature has many examples where this gives bad results
 * even with moderately sized inputs.
 *
 * See:
 *
 * B. P. Welford (1962). "Note on a method for calculating corrected sums of
 * squares and products". Technometrics 4(3):419–420.
 *
 * Chan, Tony F.; Golub, Gene H.; LeVeque, Randall J. (1979), "Updating
 * Formulae and a Pairwise Algorithm for Computing Sample Variances.", Technical
 * Report STAN-CS-79-773, Department of Computer Science, Stanford University.
 * ftp://reports.stanford.edu/pub/cstr/reports/cs/tr/79/773/CS-TR-79-773.pdf
 */
inline
void
updateCorrectedSumOfSquares(double &ioLeftWeight, double &ioLeftSum,
    double &ioLeftCorrectedSumSquares, double inRightWeight, double inRightSum,
    double inRightCorrectedSumSquares) {
    
    if (inRightWeight <= 0)
        return;
    
    // FIXME: Use compensated sums for numerical stability
    // http://jira.madlib.net/browse/MADLIB-500
    // See Ogita et al., "Accurate Sum and Dot Product", SIAM Journal on
    // Scientific Computing (SISC), 26(6):1955-1988, 2005.
    if (ioLeftWeight <= 0)
        ioLeftCorrectedSumSquares = inRightCorrectedSumSquares;
    else {
        double diff = inRightWeight / ioLeftWeight * ioLeftSum - inRightSum;
        ioLeftCorrectedSumSquares
               += inRightCorrectedSumSquares
                + ioLeftWeight / (inRightWeight * (ioLeftWeight + inRightWeight))
                    * diff * diff;
    }
    
    ioLeftSum += inRightSum;
    ioLeftWeight += inRightWeight;
}

/**
 * @brief Perform the transition step
 */
AnyType
one_way_anova_transition::run(AnyType &args) {
    OWATransitionState<MutableArrayHandle<double> > state = args[0];
    int32_t group = args[1].getAs<int32_t>();
    double value = args[2].getAs<double>();
    
    int idx = state.idxOfGroup(*this, group);
    updateCorrectedSumOfSquares(
        state.num(idx), state.sum(idx), state.corrected_square_sum(idx),
        1, value, 0);

    return state;
}

/**
 * @brief Perform the perliminary aggregation function: Merge transition states
 */
AnyType
one_way_anova_merge_states::run(AnyType &args) {
    OWATransitionState<MutableArrayHandle<double> > stateLeft = args[0];
    OWATransitionState<ArrayHandle<double> > stateRight = args[1];
    
    // Merge states together and return
    for (int posRight = 0; posRight < stateRight.numGroups; posRight++) {
        double value = stateRight.groupValues[posRight];
        int idxRight = stateRight.idxOfGroup(*this, value);
        int idxLeft = stateLeft.idxOfGroup(*this, value);
        updateCorrectedSumOfSquares(
            stateLeft.num(idxLeft), stateLeft.sum(idxLeft),
                stateLeft.corrected_square_sum(idxLeft),
            stateRight.num(idxRight), stateRight.sum(idxRight),
                stateRight.corrected_square_sum(idxRight));
    }
    
    return stateLeft;
}

/**
 * @brief Perform the one-sided t-Test final step
 */
AnyType
one_way_anova_final::run(AnyType &args) {
    OWATransitionState<ArrayHandle<double> > state = args[0];

    // If we haven't seen any data, just return Null. This is the standard
    // behavior of aggregate function on empty data sets (compare, e.g.,
    // how PostgreSQL handles sum or avg on empty inputs)
    if (state.numGroups == 0)
        return Null();
    
    double grand_mean = state.sum.sum() / state.num.sum();
    double sum_squares_between = 0;
    
    for (int idx = 0; idx < state.numGroups; idx++)
        sum_squares_between += state.num(idx)
                             * std::pow(state.sum(idx) / state.num(idx)
                                        - grand_mean, 2);
    
    double sum_squares_within = state.corrected_square_sum.sum();
    int64_t df_between = state.numGroups - 1;
    int64_t df_within = state.num.sum() - state.numGroups;
    double mean_square_between = sum_squares_between / df_between;
    double mean_square_within = sum_squares_within / df_within;
    double statistic = mean_square_between / mean_square_within;

    AnyType tuple;
    tuple
        << sum_squares_between
        << sum_squares_within
        << df_between
        << df_within
        << mean_square_between
        << mean_square_within
        << statistic
        << 1. - fisherF_CDF(statistic, df_between, df_within);
    return tuple;
}

} // namespace stats

} // namespace modules

} // namespace madlib
