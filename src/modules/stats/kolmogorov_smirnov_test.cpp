/* ----------------------------------------------------------------------- *//**
 *
 * @file kolmogorov_smirnov_test.cpp
 *
 * @brief Kolmogorov-Smirnov-test functions
 *
 *//* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>
#include <modules/shared/HandleTraits.hpp>
#include <modules/prob/kolmogorov.hpp>

#include "kolmogorov_smirnov_test.hpp"

namespace madlib {

namespace modules {

namespace stats {

/**
 * @brief Transition state for Kolmogorov-Smirnov-Test functions
 *
 * Note: We assume that the DOUBLE PRECISION array is initialized by the
 * database with length 7, and all elemenets are 0. Handle::operator[] will
 * perform bounds checking.
 */
template <class Handle>
class KSTestTransitionState {
public:
    KSTestTransitionState(const AnyType &inArray)
      : mStorage(inArray.getAs<Handle>()),
        num(&mStorage[0], 2),
        expectedNum(&mStorage[2], 2),
        last(&mStorage[4]),
        maxDiff(&mStorage[5]),
        lastDiff(&mStorage[6]) { }

    inline operator AnyType() const {
        return mStorage;
    }

private:
    Handle mStorage;

public:
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap num;
    typename HandleTraits<Handle>::ColumnVectorTransparentHandleMap expectedNum;
    typename HandleTraits<Handle>::ReferenceToDouble last;
    typename HandleTraits<Handle>::ReferenceToDouble maxDiff;
    typename HandleTraits<Handle>::ReferenceToDouble lastDiff;
};

/**
 * @brief Perform the Kolmogorov-Smirnov-test transition step
 */
AnyType
ks_test_transition::run(AnyType &args) {
    KSTestTransitionState<MutableArrayHandle<double> > state = args[0];
    int sample = args[1].getAs<bool>() ? 0 : 1;
    double value = args[2].getAs<double>();
    Eigen::Vector2d expectedNum;
        expectedNum << static_cast<double>(args[3].getAs<int64_t>()),
                       static_cast<double>(args[4].getAs<int64_t>());

    if (state.expectedNum != expectedNum) {
        if (state.num.sum() > 0)
            throw std::invalid_argument("Number of samples must be constant "
                "parameters.");

        state.expectedNum = expectedNum;
    }

    if (state.num.sum() > 0) {
        // It might actually be faster if (state.num.sum() > 0) was instead moved
        // to the end of both of the following two if-clauses (as it is a rare
        // condition). But we go for readability here.

        if (state.last > value)
            throw std::invalid_argument("Must be used as an ordered "
                "aggregate, in ascending order of the second argument.");
        else if (state.last < value && state.maxDiff < state.lastDiff)
            // We have seen the end of a group of ties, so we may now compare
            // the empirical distribution functions (conceptually, we are
            // evaluating the two empricical distribution functions at
            // state.last).
            // Note: We must wait till we have seen all rows of a group of ties.
            // (See also MADLIB-554).
            state.maxDiff = state.lastDiff;
    }
    state.num(sample)++;
    state.last = value;

    state.lastDiff = std::fabs(state.num(0) / state.expectedNum(0)
                    - state.num(1) / state.expectedNum(1));

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
    using boost::math::complement;

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

    // Note that at this point we also have state.lastDiff == 0 and thus
    // state.lastDiff <= state.maxDiff.

    double root = std::sqrt(state.num.prod() / state.num.sum());
    double kolmogorov_statistic = (root + 0.12 + 0.11 / root) * state.maxDiff;

    AnyType tuple;
    tuple
        << static_cast<double>(state.maxDiff) // The Kolmogorov-Smirnov statistic
        << kolmogorov_statistic
        << prob::cdf(complement(prob::kolmogorov(), kolmogorov_statistic));
    return tuple;
}

} // namespace stats

} // namespace modules

} // namespace madlib
