/* ----------------------------------------------------------------------- *//**
 *
 * @file mann_whitney_test.cpp
 *
 * @brief Mann-Whitney-U-test functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/prob/boost.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <utils/Math.hpp>

#include "mann_whitney_test.hpp"

namespace madlib {

namespace modules {

namespace stats {

/**
 * @brief Transition state for Mann-Whitney-Test functions
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 7, and all elemenets are 0. Handle::operator[] will
 * perform bounds checking.
 */
template <class Handle>
class MWTestTransitionState {
public:
    MWTestTransitionState(const AnyType &inArray)
      : mStorage(inArray.getAs<Handle>()),
        num(&mStorage[0], 2),
        numTies(&mStorage[2], 2),
        rankSum(&mStorage[4], 2),
        last(&mStorage[6]) { }
    
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
};

/**
 * @brief Perform the Mann-Whitney-test transition step
 */
AnyType
mw_test_transition::run(AnyType &args) {
    MWTestTransitionState<MutableArrayHandle<double> > state = args[0];
    int sample = args[1].getAs<bool>() ? 0 : 1;
    double value = args[2].getAs<double>();
    
    // For almostEqual, we choose a precision of 2 * 1 units in the last place.
    // This is because we assume that value is original data, so the only
    // precision loss is due to representation as floating-point number.
    if (utils::almostEqual(static_cast<double>(state.last), value, 2)) {
        for (int i = 0; i <= 1; i++)
            state.rankSum(i) += state.numTies(i) * 0.5;
    } else if (state.last < value) {
        state.numTies.setZero();
    } else if (state.num.sum() > 0) {
        // also satisfied here: state.last > value
        throw std::invalid_argument("Must be used as an ordered aggregate, "
            "in ascending order of the second argument.");
    }
    
    state.num(sample)++;
    state.rankSum(sample) += (2. * state.num.sum() - state.numTies.sum()) / 2.;
    state.numTies(sample)++;
    state.last = value;
    
    return state;
}

AnyType
mw_test_final::run(AnyType &args) {
    using boost::math::complement;

    MWTestTransitionState<ArrayHandle<double> > state = args[0];

    Eigen::Vector2d U;
    double numProd = state.num.prod();
    
    U(0) = state.rankSum(1) - state.num(1) * (state.num(1) + 1.) / 2.;
    U(1) = numProd - U(0);
    
    double u_statistic = U.minCoeff();
    double z_statistic = (u_statistic - (numProd / 2.))
                       / (std::sqrt( numProd * (state.num.sum() + 1) / 12. ));
    
    AnyType tuple;
    tuple
        << z_statistic
        << u_statistic
        << prob::cdf(complement(prob::normal(), z_statistic))
        << 2. * prob::cdf(complement(prob::normal(), std::fabs(z_statistic)));
    return tuple;
}

} // namespace stats

} // namespace modules

} // namespace madlib
