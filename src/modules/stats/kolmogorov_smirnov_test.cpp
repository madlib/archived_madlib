/* ----------------------------------------------------------------------- *//**
 *
 * @file kolmogorov_smirnov_test.cpp
 *
 * @brief Kolmogorov-Smirnov-test functions
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
using prob::kolmogorovCDF;

namespace stats {

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "kolmogorov_smirnov_test.hpp"

/**
 * @brief Transition state for Kolmogorov-Smirnov-Test functions
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 7, and all elemenets are 0.
 */
template <class Handle>
class KSTestTransitionState : public AbstractionLayer {
public:
    KSTestTransitionState(const AnyType &inArray)
      : mStorage(inArray.getAs<Handle>()),
        num(&mStorage[0], 2),
        last(&mStorage[2]),
        maxDiff(&mStorage[3]),
        expectedNum(&mStorage[4], 2) { }
    
    inline operator AnyType() const {
        return mStorage;
    }
    
private:
    Handle mStorage;

public:
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap num;
    typename HandleTraits<Handle>::ReferenceToDouble last;
    typename HandleTraits<Handle>::ReferenceToDouble maxDiff;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap expectedNum;
};

/**
 * @brief Perform the Kolmogorov-Smirnov-test transition step
 */
AnyType
ks_test_transition::run(AnyType &args) {
    KSTestTransitionState<MutableArrayHandle<double> > state = args[0];
    int sample = args[1].getAs<bool>() ? 0 : 1;
    double value = args[2].getAs<double>();
    ColumnVector2 expectedNum;
        expectedNum << args[3].getAs<int64_t>(), args[4].getAs<int64_t>();
    
    if (state.expectedNum != expectedNum) {
        if (state.num.sum() > 0)
            throw std::invalid_argument("Number of samples must be constant "
                "parameters.");
        
        state.expectedNum = expectedNum;
    }
    
    if (state.last > value && state.num.sum() > 0)
        throw std::invalid_argument("Must be used as an ordered aggregate.");
    state.num(sample)++;
    state.last = value;
    
    double diff = std::fabs(state.num(0) / state.expectedNum(0)
                    - state.num(1) / state.expectedNum(1));
    if (state.maxDiff < diff)
        state.maxDiff = diff;
    
    return state;
}

/**
 * @brief Perform the Kolmogorov-Smirnov-test final step
 *
 * Define \f$ N := \frac{n_1 n_2}{n_1 + n_2} \f$ and
 * \f$ D := \max_x |F_1(x) - F_2(x)| \f$ where
 * \f$ F_i(x) = \frac{ |\{ j \mid x_{i,j} < x \}|  }{n_i} \f$
 * is the empirical distribution of \f$ x_{i,1}, \dots, x_{i, n_i} \f$.
 * The test statistic is computed as \f$ \sqrt N \cdot D \f$.
 *
 * The p-value is computed as
 * \f[
 *     F_{\text{KS}}( (\sqrt N + 0.12 + \frac{0.11}{\sqrt N}) \cdot D )
 * \f]
 * where \f$ F_{\text{KS}} \f$ is the cumulative distribution function of the
 * Kolmogorov-Smirnov distribution. This is suggested by:
 *
 * M. A. Stephens, "Use of the Kolmogorov-Smirnov, Cramer-Von Mises and Related 
 * Statistics Without Extensive Tables", Journal of the Royal Statistical
 * Society. Series B (Methodological), Vol. 32, No. 1. (1970), pp. 115-122.
 */
AnyType
ks_test_final::run(AnyType &args) {
    KSTestTransitionState<ArrayHandle<double> > state = args[0];

    if (state.num != state.expectedNum) {
        std::stringstream tmp;
        tmp << "Actual sample sizes differ from specified sizes. "
                "Actual/specified: "
            << uint64_t(state.num(0)) << "/" << uint64_t(state.expectedNum(0))
            << " and "
            << uint64_t(state.num(1)) << "/" << uint64_t(state.expectedNum(1));
        throw std::invalid_argument(tmp.str());
    }

    double root = std::sqrt(state.num.prod() / state.num.sum());
    double kolmogorov_statistic = (root + 0.12 + 0.11 / root) * state.maxDiff;

    AnyType tuple;
    tuple
        << static_cast<double>(state.maxDiff) // The Kolmogorov-Smirnov statistic
        << kolmogorov_statistic
        << 1. - kolmogorovCDF( kolmogorov_statistic );
    return tuple;
}

} // namespace stats

} // namespace modules

} // namespace madlib
