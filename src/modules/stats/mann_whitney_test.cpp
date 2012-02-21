/* ----------------------------------------------------------------------- *//**
 *
 * @file mann_whitney_test.cpp
 *
 * @brief Mann-Whitney-U-test functions
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
#include "mann_whitney_test.hpp"

/**
 * @brief Transition state for Mann-Whitney-Test functions
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 7, and all elemenets are 0.
 */
template <class Handle>
class MWTestTransitionState : public AbstractionLayer {
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
    
    if (state.last < value) {
        state.numTies.setZero();
    } else if (state.last == value) {
        for (int i = 0; i <= 1; i++)
            state.rankSum(i) += state.numTies(i) * 0.5;
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
mw_test_final::run(AnyType &args) {
    MWTestTransitionState<ArrayHandle<double> > state = args[0];

    ColumnVector2 U;
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
        << 1. - normalCDF(z_statistic)
        << 2. * (1. - normalCDF(std::fabs(z_statistic)));
    return tuple;
}

} // namespace stats

} // namespace modules

} // namespace madlib
