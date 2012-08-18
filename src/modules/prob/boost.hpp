/* ----------------------------------------------------------------------- *//**
 *
 * @file boost.hpp
 *
 *//* ----------------------------------------------------------------------- */

#define LIST_CONTINUOUS_PROB_DISTR \
    MADLIB_ITEM(beta) \
    MADLIB_ITEM(cauchy) \
    MADLIB_ITEM(chi_squared) \
    MADLIB_ITEM(fisher_f) \
    MADLIB_ITEM(exponential) \
    MADLIB_ITEM(extreme_value) \
    MADLIB_ITEM(gamma) \
    MADLIB_ITEM(inverse_chi_squared) \
    MADLIB_ITEM(inverse_gamma) \
    MADLIB_ITEM(laplace) \
    MADLIB_ITEM(logistic) \
    MADLIB_ITEM(lognormal) \
    MADLIB_ITEM(non_central_beta) \
    MADLIB_ITEM(non_central_chi_squared) \
    MADLIB_ITEM(non_central_f) \
    MADLIB_ITEM(non_central_t) \
    MADLIB_ITEM(normal) \
    MADLIB_ITEM(pareto) \
    MADLIB_ITEM(rayleigh) \
    MADLIB_ITEM(triangular) \
    MADLIB_ITEM(uniform) \
    MADLIB_ITEM(weibull)
// FIXME: MADLIB-513: Pending Boost bug 6934, we currently do not support the
// inverse Gaussian distribution. https://svn.boost.org/trac/boost/ticket/6934
//    MADLIB_ITEM(inverse_gaussian)
// For Student's t distribution, see student.hpp


#define LIST_DISCRETE_PROB_DISTR \
    MADLIB_ITEM(bernoulli) \
    MADLIB_ITEM(binomial) \
    MADLIB_ITEM(geometric) \
    MADLIB_ITEM(hypergeometric) \
    MADLIB_ITEM(negative_binomial) \
    MADLIB_ITEM(poisson)


#define MADLIB_ITEM(dist) \
    DECLARE_UDF(prob, dist ## _cdf) \
    DECLARE_UDF(prob, dist ## _pdf) \
    DECLARE_UDF(prob, dist ## _quantile)

LIST_CONTINUOUS_PROB_DISTR

#undef MADLIB_ITEM

#define MADLIB_ITEM(dist) \
    DECLARE_UDF(prob, dist ## _cdf) \
    DECLARE_UDF(prob, dist ## _pmf) \
    DECLARE_UDF(prob, dist ## _quantile)

LIST_DISCRETE_PROB_DISTR

#undef MADLIB_ITEM


#ifndef MADLIB_MODULES_PROB_BOOST_HPP
#define MADLIB_MODULES_PROB_BOOST_HPP

#include <boost/math/distributions.hpp>

namespace madlib {

namespace modules {

namespace prob {

namespace {
// No need to make this visable beyond this translation unit.

enum ProbFnOverride {
    kResultIsReady = 0,
    kLetBoostCalculate,
    kLetBoostCalculateUsingValue
};

/**
 * @brief Via (partial) specialization, this class offers a way to override
 *     boost's domain checks
 *
 * Some boost functions have domain checks we would like to override. E.g.,
 * boost's CDF and PDF for the Fisher F-distribution raise a domain error if the
 * input argument is <0 or infinity. By using madlib::modules::prob::cdf() (and
 * pdf/quantile, respectively), these domain checks can be overridden.
 * In MADlib, we always prefer returning correct values as opposed to raising
 * errors. E.g., for the CDF, it is the correct mathematical behavior to simply
 * return 0 if the input argument for the Fisher F-distribution is < 0.
 *
 * The following functions return \c true if boost's implementation should be
 * called, and they return \c false if the function result has alrady been
 * stored in \c outResult and boost's implementation should not be called any
 * more.
 *
 * Note that C++03 does not support partial specialization of functions. We
 * therefore must partially specialize the whole \c DomainCheck class.
 */
template <class Distribution>
struct DomainCheck {
    typedef typename Distribution::value_type RealType;

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution&, const RealType&, RealType&) {
        return kLetBoostCalculate;
    }

    static ProbFnOverride pdf(const Distribution&, const RealType&, RealType&) {
        return kLetBoostCalculate;
    }

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution&, const RealType&,
        RealType&) {

        return kLetBoostCalculate;
    }
};

/**
 * @brief Domain-check overrides for distribution functions with support in
 *     \f$ \mathbb R \f$
 *
 * We override the domain check by treating -infinity and infinity as part of
 * the domain. Some, but not all, boost functions would raise a domain_error
 * instead. See:
 * http://www.boost.org/doc/libs/1_49_0/libs/math/doc/sf_and_dist/html/math_toolkit/backgrounders/implementation.html#math_toolkit.backgrounders.implementation.handling_of_floating_point_infinity
 */
template <class Distribution>
struct RealDomainCheck {
    typedef typename Distribution::value_type RealType;

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution&, const RealType& inX,
        RealType& outResult) {

        if (boost::math::isinf(inX)) {
            if (inX < 0)
                outResult = Complement ? 1 : 0;
            else
                outResult = Complement ? 0 : 1;
            return kResultIsReady;
        }
        return kLetBoostCalculate;
    }

    static ProbFnOverride pdf(const Distribution&, const RealType& inX,
        RealType& outResult) {

        if (boost::math::isinf(inX)) {
            outResult = 0;
            return kResultIsReady;
        }
        return kLetBoostCalculate;
    }

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution&, const RealType&, RealType&) {
        return kLetBoostCalculate;
    }
};

/**
 * @brief Domain-check overrides for distribution functions with support in
 *     \f$ [0, \infty) \f$
 */
template <class Distribution>
struct PositiveDomainCheck : public RealDomainCheck<Distribution> {
    typedef RealDomainCheck<Distribution> Base;
    typedef typename Base::RealType RealType;

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        if (inX < 0) {
            outResult = Complement ? 1 : 0;
            return kResultIsReady;
        }
        return Base::template cdf<Complement>(inDist, inX, outResult);
    }

    static ProbFnOverride pdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        if (inX < 0) {
            outResult = 0;
            return kResultIsReady;
        }
        return Base::pdf(inDist, inX, outResult);
    }
};

/**
 * @brief Domain-check overrides for distribution functions with support in
 *     \f$ [0, 1] \f$
 */
template <class Distribution>
struct ZeroOneDomainCheck : public PositiveDomainCheck<Distribution> {
    typedef PositiveDomainCheck<Distribution> Base;
    typedef typename Base::RealType RealType;

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution&, const RealType& inX,
        RealType& outResult) {

        if (inX < 0)
            outResult = Complement ? 1 : 0;
        else if (inX > 1)
            outResult = Complement ? 0 : 1;
        else
            return kLetBoostCalculate;

        return kResultIsReady;
    }

    static ProbFnOverride pdf(const Distribution&, const RealType& inX,
        RealType& outResult) {

        if (inX < 0 || inX > 1) {
            outResult = 0;
            return kResultIsReady;
        }
        return kLetBoostCalculate;
    }
};

/**
 * @brief Domain-check overrides for distribution functions with support in
 *     \f$ \mathbb Z \f$
 */
template <class Distribution>
struct IntegerDomainCheck : public RealDomainCheck<Distribution> {
    typedef RealDomainCheck<Distribution> Base;
    typedef typename Base::RealType RealType;

    static ProbFnOverride internalMakeIntegral(ProbFnOverride inAction,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "IntegerDomainCheck::internalMakeIntegral(...)";

        if (inAction != kResultIsReady) {
            if (std::isnan(inX)) {
                outResult = boost::math::policies::raise_domain_error<RealType>(
                    function,
                    "Random variate must be integral but was: %1%.",
                    inX,
                    typename Distribution::policy_type());
                inAction = kResultIsReady;
            } else {
                outResult = std::floor(inX);
                inAction = kLetBoostCalculateUsingValue;
            }
        }
        return inAction;
    }

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        return internalMakeIntegral(
            Base::template cdf<Complement>(inDist, inX, outResult), inX,
                outResult);
    }

    static ProbFnOverride pdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        return internalMakeIntegral(
            Base::pdf(inDist, inX, outResult), inX, outResult);
    }
};

/**
 * @brief Domain-check overrides for distribution functions with support in
 *     \f$ \mathbb N_0 \f$
 */
template <class Distribution>
struct NonNegativeIntegerDomainCheck : public IntegerDomainCheck<Distribution> {
    typedef IntegerDomainCheck<Distribution> Base;
    typedef typename Base::RealType RealType;

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        if (inX < 0) {
            outResult = Complement ? 1 : 0;
            return kResultIsReady;
        }
        return Base::template cdf<Complement>(inDist, inX, outResult);
    }

    static ProbFnOverride pdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        if (inX < 0) {
            outResult = 0;
            return kResultIsReady;
        }
        return Base::pdf(inDist, inX, outResult);
    }
};


/**
 * @brief Boost only accepts 0 or 1 for random variate
 *
 * Due to boost bug 6937, we also need to override the domain check for quantile
 *
 * https://svn.boost.org/trac/boost/ticket/6937
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::bernoulli_distribution<RealType, Policy> >
  : public IntegerDomainCheck<
        boost::math::bernoulli_distribution<RealType, Policy>
    > {
    typedef boost::math::bernoulli_distribution<RealType, Policy> Distribution;
    typedef IntegerDomainCheck<Distribution> Base;

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        if (inX < 0)
            outResult = Complement ? 1 : 0;
        else if (inX > 1)
            outResult = Complement ? 0 : 1;
        else
            return Base::template cdf<Complement>(inDist, inX, outResult);

        return kResultIsReady;
    }

    static ProbFnOverride pdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        if (inX < 0 || inX > 1) {
            outResult = 0;
            return kResultIsReady;
        } else
            return Base::pdf(inDist, inX, outResult);
    }

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<bernoulli_distribution<%1%> >::quantile(...)";

        if (!boost::math::detail::check_probability(function,
            inDist.success_fraction(), &outResult, Policy()))
            return kResultIsReady;

        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};

/**
 * @brief Boost only accepts a limited range for random variates
 *
 * Due to boost bug 6937, we also need to override the domain check for quantile
 *
 * https://svn.boost.org/trac/boost/ticket/6937
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::binomial_distribution<RealType, Policy> >
  : public IntegerDomainCheck<
        boost::math::binomial_distribution<RealType, Policy>
    > {
    typedef boost::math::binomial_distribution<RealType, Policy> Distribution;
    typedef IntegerDomainCheck<Distribution> Base;

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<binomial_distribution<%1%> >::cdf(...)";

        if (!boost::math::binomial_detail::check_dist(function, inDist.trials(),
            inDist.success_fraction(), &outResult, Policy())) {
            return kResultIsReady;
        } else if (inX < 0) {
            outResult = Complement ? 1 : 0;
            return kResultIsReady;
        } else if (inX > inDist.trials()) {
            outResult = Complement ? 0 : 1;
            return kResultIsReady;
        }
        return Base::template cdf<Complement>(inDist, inX, outResult);
    }

    static ProbFnOverride pdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<binomial_distribution<%1%> >::pdf(...)";

        if (!boost::math::binomial_detail::check_dist(function, inDist.trials(),
            inDist.success_fraction(), &outResult, Policy())) {
            return kResultIsReady;
        } else if (inX < 0 || inX > inDist.trials()) {
            outResult = 0;
            return kResultIsReady;
        }
        return Base::pdf(inDist, inX, outResult);
    }

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<binomial_distribution<%1%> >::quantile(...)";

        if (!boost::math::binomial_detail::check_dist(function, inDist.trials(),
                inDist.success_fraction(), &outResult, Policy())
            || !boost::math::detail::check_probability(function, inP,
                &outResult, Policy())) {
            return kResultIsReady;
        } else if (inDist.success_fraction() == 1) {
            // distribution is single-point measure, i.e., same for complement
            outResult = inDist.trials();
            return kResultIsReady;
        } else if (inDist.success_fraction() == 0) {
            // distribution is single-point measure, i.e., same for complement
            outResult = 0;
            return kResultIsReady;
        }
        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};

/**
 * @brief Due to boost bug XXX, we need to override the domain checks
 *
 * FIXME: No boost bug filed so far
 * Boost does not catch the case where lambda is non-finite.
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::exponential_distribution<RealType, Policy> >
  : public PositiveDomainCheck<
        boost::math::exponential_distribution<RealType, Policy>
    > {
    typedef boost::math::exponential_distribution<RealType, Policy> Distribution;
    typedef PositiveDomainCheck<Distribution> Base;

    static bool check_dist(const char* function, RealType lambda,
        RealType* result, const Policy& pol) {

        if (!boost::math::isfinite(lambda)) {
            *result = boost::math::policies::raise_domain_error<RealType>(
                function,
                "The scale parameter \"lambda\" must be finite, but was: %1%.",
                lambda, pol);
            return false;
        }
        return true;
    }

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<exponential_distribution<%1%> >::cdf(...)";

        if (!check_dist(function, inDist.lambda(), &outResult, Policy()))
            return kResultIsReady;
        return Base::template cdf<Complement>(inDist, inX, outResult);
    }

    static ProbFnOverride pdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<exponential_distribution<%1%> >::pdf(...)";

        if (!check_dist(function, inDist.lambda(), &outResult, Policy()))
            return kResultIsReady;
        return Base::pdf(inDist, inX, outResult);
    }

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<exponential_distribution<%1%> >::quantile(...)";

        if (!check_dist(function, inDist.lambda(), &outResult, Policy()))
            return kResultIsReady;
        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};

/**
 * @brief Due to boost bug XXX, we need to override the domain check for
 *     quantile
 *
 * FIXME: No boost bug filed so far
 * Boost does not catch the case where the location or scale parameters are
 * non-finite.
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::extreme_value_distribution<RealType, Policy> >
  : public RealDomainCheck<
        boost::math::extreme_value_distribution<RealType, Policy>
    > {
    typedef boost::math::extreme_value_distribution<RealType, Policy> Distribution;
    typedef RealDomainCheck<Distribution> Base;

    static bool check_dist(const char* function, RealType location,
        RealType scale, RealType* result, const Policy& pol) {

        return
            boost::math::detail::check_location(function, location, result, pol)
            && boost::math::detail::check_scale(function, scale, result, pol);
    }

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<extreme_value_distribution<%1%> >::cdf(...)";

        if (!check_dist(function, inDist.location(), inDist.scale(), &outResult,
            Policy()))
            return kResultIsReady;
        return Base::template cdf<Complement>(inDist, inX, outResult);
    }

    static ProbFnOverride pdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<extreme_value_distribution<%1%> >::pdf(...)";

        if (!check_dist(function, inDist.location(), inDist.scale(), &outResult,
            Policy()))
            return kResultIsReady;
        return Base::pdf(inDist, inX, outResult);
    }

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<extreme_value_distribution<%1%> >::quantile(...)";

        if (!check_dist(function, inDist.location(), inDist.scale(), &outResult,
            Policy()))
            return kResultIsReady;
        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};

/**
 * @brief Due to boost bug 6937, we need to override the domain check for
 *     quantile
 *
 * https://svn.boost.org/trac/boost/ticket/6937
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::fisher_f_distribution<RealType, Policy> >
  : public PositiveDomainCheck<
        boost::math::fisher_f_distribution<RealType, Policy>
    > {
    typedef boost::math::fisher_f_distribution<RealType, Policy> Distribution;
    typedef PositiveDomainCheck<Distribution> Base;

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<fisher_f_distribution<%1%> >::quantile(...)";

        if (!boost::math::detail::check_df(function,
            inDist.degrees_of_freedom1(), &outResult, Policy())
            || !boost::math::detail::check_df(function,
                inDist.degrees_of_freedom2(), &outResult, Policy()))
            return kResultIsReady;

        if (std::isnan(inP)) {
            outResult = boost::math::policies::raise_domain_error<RealType>(
                function,
                "Probability argument is %1%, but must be >= 0 and <= 1!", inP,
                Policy());
            return kResultIsReady;
        }
        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};

/**
 * @brief Due to boost bug XXXX, we override the domain check for gamma: pdf
 *
 * For the gamma distribution, boost's pdf always returns 0 for x = 0. That is
 * wrong.
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::gamma_distribution<RealType, Policy> >
  : public PositiveDomainCheck<
        boost::math::gamma_distribution<RealType, Policy>
    > {
    typedef boost::math::gamma_distribution<RealType, Policy> Distribution;
    typedef PositiveDomainCheck<Distribution> Base;

    static ProbFnOverride pdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<gamma_distribution<%1%> >::pdf(...)";
        RealType shape = inDist.shape();
        RealType scale = inDist.scale();
        if (!boost::math::detail::check_gamma(function, scale, shape,
            &outResult, Policy()))
            return kResultIsReady;

        if (inX == 0) {
            if (shape == 1)
                outResult = 1. / scale;
            else if (shape < 1)
                outResult
                    = boost::math::policies::raise_overflow_error<RealType>(
                        function, 0, Policy());
            else
                return kLetBoostCalculate;

            return kResultIsReady;
        }
        return Base::pdf(inDist, inX, outResult);
    }
};

/**
 * @brief Due to boost bug 6937, we need to override the domain check for
 *     quantile
 *
 * https://svn.boost.org/trac/boost/ticket/6937
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::geometric_distribution<RealType, Policy> >
  : public NonNegativeIntegerDomainCheck<
        boost::math::geometric_distribution<RealType, Policy>
    > {
    typedef boost::math::geometric_distribution<RealType, Policy> Distribution;
    typedef NonNegativeIntegerDomainCheck<Distribution> Base;

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        if (inDist.success_fraction() == 1 && !std::isnan(inX)) {
            if (inX < 0)
                outResult = Complement ? 1 : 0;
            else if (inX > 0)
                outResult = Complement ? 0 : 1;
            else /* if (inX == 0) */
                outResult = 1;
            return kResultIsReady;
        }
        return Base::template cdf<Complement>(inDist, inX, outResult);
    }

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<geometric_distribution<%1%> >::quantile(...)";

        if (!boost::math::detail::check_probability(function,
            inDist.success_fraction(), &outResult, Policy()))
            return kResultIsReady;

        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};


/**
 * @brief Boost only accepts a limited range for random variates
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::hypergeometric_distribution<RealType, Policy> >
  : public NonNegativeIntegerDomainCheck<
        boost::math::hypergeometric_distribution<RealType, Policy>
    > {
    typedef boost::math::hypergeometric_distribution<RealType, Policy> Distribution;
    typedef NonNegativeIntegerDomainCheck<Distribution> Base;

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        // inDist's parameters are all unsigned, so we need to be careful in the
        // comparison!
        if (inX + inDist.total() < inDist.defective() + inDist.sample_count())
            outResult = Complement ? 1 : 0;
        else if (inX > inDist.sample_count() || inX > inDist.defective())
            outResult = Complement ? 0 : 1;
        else
            return Base::template cdf<Complement>(inDist, inX, outResult);

        return kResultIsReady;
    }

    static ProbFnOverride pdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        // inDist's parameters are all unsigned, so we need to be careful in the
        // comparison!
        if (inX + inDist.total() < inDist.defective() + inDist.sample_count()
            || inX > inDist.sample_count() || inX > inDist.defective()) {
            outResult = 0;
            return kResultIsReady;
        } else
            return Base::pdf(inDist, inX, outResult);
    }
};


/**
 * @brief Boost returns a small non-zero value for quantile(0) instead of 0
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::inverse_gamma_distribution<RealType, Policy> >
  : public PositiveDomainCheck<
        boost::math::inverse_gamma_distribution<RealType, Policy>
    > {
    typedef boost::math::inverse_gamma_distribution<RealType, Policy> Distribution;
    typedef PositiveDomainCheck<Distribution> Base;

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<inverse_gamma_distribution<%1%> >::quantile(...)";

        if(!boost::math::detail::check_inverse_gamma(function, inDist.scale(),
            inDist.shape(), &outResult, Policy()))
            return kResultIsReady;
        else if (inP == 0) {
            outResult = 0;
            return kResultIsReady;
        }
        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};

/**
 * @brief Due to boost bug XXX, we need to override the domain check for
 *     quantile
 *
 * FIXME: No boost bug filed so far
 * Boost does not catch the case where location or scale are not finite.
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::lognormal_distribution<RealType, Policy> >
  : public PositiveDomainCheck<
        boost::math::lognormal_distribution<RealType, Policy>
    > {
    typedef boost::math::lognormal_distribution<RealType, Policy> Distribution;
    typedef PositiveDomainCheck<Distribution> Base;

    static bool check_dist(const char* function, RealType location,
        RealType scale, RealType* result, const Policy& pol) {

        if (!boost::math::detail::check_location(function, location, result,
                pol)
            || !boost::math::detail::check_scale(function, scale, result, pol))
            return false;
        return true;
    }

    static ProbFnOverride pdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<lognormal_distribution<%1%> >::pdf(...)";

        if (!check_dist(function, inDist.location(), inDist.scale(), &outResult,
            Policy()))
            return kResultIsReady;
        return Base::pdf(inDist, inX, outResult);
    }

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<lognormal_distribution<%1%> >::quantile(...)";

        if (!check_dist(function, inDist.location(), inDist.scale(), &outResult,
            Policy()))
            return kResultIsReady;
        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};

/**
 * @brief Due to boost bug 6937, we need to override the domain check for
 *     quantile.
 *
 * https://svn.boost.org/trac/boost/ticket/6937
 *
 * Also, we want to raise an error if the success probability is 0, because the
 * distribution is not well-defined in that case.
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::negative_binomial_distribution<RealType, Policy> >
  : public NonNegativeIntegerDomainCheck<
        boost::math::negative_binomial_distribution<RealType, Policy>
    > {
    typedef boost::math::negative_binomial_distribution<RealType, Policy> Distribution;
    typedef NonNegativeIntegerDomainCheck<Distribution> Base;

    static bool check_dist(const char* function, const RealType& r,
        const RealType& p, RealType* result, const Policy& pol) {

        if (!boost::math::negative_binomial_detail::check_dist(function, r, p,
                result, pol)) {
            return false;
        } else if (p == 0) {
            *result = boost::math::policies::raise_domain_error<RealType>(
                function,
                "Probability argument is %1%, but must be > 0 and <= 1!",
                p, pol);
            return false;
        }
        return true;
    }

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<negative_binomial_distribution<%1%> >::cdf(...)";

        if (!check_dist(function, inDist.successes(), inDist.success_fraction(),
            &outResult, Policy()))
            return kResultIsReady;
        return Base::template cdf<Complement>(inDist, inX, outResult);
    }

    static ProbFnOverride pdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<negative_binomial_distribution<%1%> >::pdf(...)";

        if (!check_dist(function, inDist.successes(), inDist.success_fraction(),
            &outResult, Policy()))
            return kResultIsReady;
        return Base::pdf(inDist, inX, outResult);
    }

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<negative_binomial_distribution<%1%> >::quantile(...)";

        if (!check_dist(function, inDist.successes(), inDist.success_fraction(),
            &outResult, Policy())) {
            return kResultIsReady;
        } else if (inDist.success_fraction() == 1) {
            // distribution is single-point measure, i.e., same for complement
            outResult = 0;
            return kResultIsReady;
        }
        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};

/**
 * @brief Due to boost bug XXX, we need to check the corner cases for the pdf
 *
 * FIXME: No boost bug filed so far
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::non_central_beta_distribution<RealType, Policy> >
  : public ZeroOneDomainCheck<
        boost::math::non_central_beta_distribution<RealType, Policy>
    > {
    typedef boost::math::non_central_beta_distribution<RealType, Policy> Distribution;
    typedef ZeroOneDomainCheck<Distribution> Base;

    static ProbFnOverride pdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<non_central_beta_distribution<%1%> >::quantile(...)";

        if (inX == 0 || inX == 1) {
            if(!boost::math::beta_detail::check_alpha(function, inDist.alpha(),
                    &outResult, Policy())
                || !boost::math::beta_detail::check_beta(function,
                    inDist.beta(), &outResult, Policy())
                || !boost::math::detail::check_non_centrality(function,
                    inDist.non_centrality(), &outResult, Policy()))
                return kResultIsReady;

            if (inX == 0) {
                if (inDist.alpha() < 1)
                    outResult = boost::math::policies
                        ::raise_overflow_error<RealType>(function, 0, Policy());
                else if (inDist.alpha() == 1)
                    outResult = inDist.beta()
                              * std::exp(-inDist.non_centrality()/2.);
                else /* if (inDist.alpha() > 1) */
                    outResult = 0;
                return kResultIsReady;
            } else /* if (inX == 1) */ {
                if (inDist.beta() < 1)
                    outResult = boost::math::policies
                        ::raise_overflow_error<RealType>(function, 0, Policy());
                else if (inDist.beta() == 1)
                    outResult = inDist.alpha() + inDist.non_centrality()/2.;
                else /* if (inDist.beta() > 1) */
                    outResult = 0;
                return kResultIsReady;
            }
        }
        return Base::pdf(inDist, inX, outResult);
    }
};

/**
 * @brief Due to boost bug XXX, we need to check the corner cases for the pdf
 *
 * FIXME: No boost bug filed so far
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::non_central_chi_squared_distribution<RealType, Policy> >
  : public PositiveDomainCheck<
        boost::math::non_central_chi_squared_distribution<RealType, Policy>
    > {
    typedef boost::math::non_central_chi_squared_distribution<RealType, Policy> Distribution;
    typedef PositiveDomainCheck<Distribution> Base;

    static ProbFnOverride pdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<non_central_chi_squared_distribution<%1%> >::quantile(...)";

        if (inX == 0) {
            if(!boost::math::detail::check_df(function,
                    inDist.degrees_of_freedom(), &outResult, Policy())
                || !boost::math::detail::check_non_centrality(function,
                    inDist.non_centrality(), &outResult, Policy()))
                return kResultIsReady;

            if (inDist.degrees_of_freedom() < 2)
                outResult = boost::math::policies
                    ::raise_overflow_error<RealType>(function, 0, Policy());
            else if (inDist.degrees_of_freedom() == 2)
                // In this case, f(x) = exp(-lambda/2) * g(x), where g(x)
                // is the densitiy of a chi-squared distributed RV with 2
                // degrees of freedom, i.e., f(x) = exp(-lambda/2) / 2
                outResult = std::exp(-inDist.non_centrality()/2.) / 2.;
            else /* if (inDist.degrees_of_freedom() > 2) */
                outResult = 0;
            return kResultIsReady;
        }
        return Base::pdf(inDist, inX, outResult);
    }
};

/**
 * @brief Due to boost bug XXX, we need to check the corner cases for the pdf
 *
 * FIXME: No boost bug filed so far
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::non_central_f_distribution<RealType, Policy> >
  : public PositiveDomainCheck<
        boost::math::non_central_f_distribution<RealType, Policy>
    > {
    typedef boost::math::non_central_f_distribution<RealType, Policy> Distribution;
    typedef PositiveDomainCheck<Distribution> Base;

    static ProbFnOverride pdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<non_central_f_distribution<%1%> >::quantile(...)";

        if (inX == 0) {
            if(!boost::math::detail::check_df(function,
                    inDist.degrees_of_freedom1(), &outResult, Policy())
                || !boost::math::detail::check_df(function,
                    inDist.degrees_of_freedom2(), &outResult, Policy())
                || !boost::math::detail::check_non_centrality(function,
                    inDist.non_centrality(), &outResult, Policy()))
                return kResultIsReady;

            if (inDist.degrees_of_freedom1() < 2)
                outResult = boost::math::policies
                    ::raise_overflow_error<RealType>(function, 0, Policy());
            else if (inDist.degrees_of_freedom1() == 2)
                // In this case, f(x) = exp(-lambda/2)
                outResult = std::exp(-inDist.non_centrality()/2.);
            else /* if (inDist.degrees_of_freedom1() > 2) */
                outResult = 0;
            return kResultIsReady;
        }
        return Base::pdf(inDist, inX, outResult);
    }
};

/**
 * @brief Override the domain check for Pareto: quantile
 *
 * For the Pareto distribution, boost sometimes returns max_value instead
 * of infinity. We override that.
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::pareto_distribution<RealType, Policy> >
  : public PositiveDomainCheck<
        boost::math::pareto_distribution<RealType, Policy>
    > {
    typedef boost::math::pareto_distribution<RealType, Policy> Distribution;
    typedef PositiveDomainCheck<Distribution> Base;

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<pareto_distribution<%1%> >::cdf(...)";

        if (inX <= inDist.scale()) {
            if (boost::math::detail::check_pareto(function, inDist.scale(),
                inDist.shape(), &outResult, Policy()))
                outResult = 0;
            return kResultIsReady;
        }
        return Base::template cdf<Complement>(inDist, inX, outResult);
    }

    static ProbFnOverride pdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<pareto_distribution<%1%> >::pdf(...)";

        if (inX < inDist.scale()) {
            if (boost::math::detail::check_pareto(function, inDist.scale(),
                inDist.shape(), &outResult, Policy()))
                outResult = 0;
            return kResultIsReady;
        }
        return Base::pdf(inDist, inX, outResult);
    }

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<pareto_distribution<%1%> >::quantile(...)";

        if (!boost::math::detail::check_pareto(function, inDist.scale(),
            inDist.shape(), &outResult, Policy()))
            return kResultIsReady;
        else if (inP == 1) {
            outResult = boost::math::policies::raise_overflow_error<RealType>(
                function, 0, Policy());
            return kResultIsReady;
        }
        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};

/**
 * @brief Due to boost bug 6937, we need to override the domain check for
 *     quantile
 *
 * https://svn.boost.org/trac/boost/ticket/6937
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::poisson_distribution<RealType, Policy> >
  : public NonNegativeIntegerDomainCheck<
        boost::math::poisson_distribution<RealType, Policy>
    > {
    typedef boost::math::poisson_distribution<RealType, Policy> Distribution;
    typedef NonNegativeIntegerDomainCheck<Distribution> Base;

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<poisson_distribution<%1%> >::quantile(...)";

        if (!boost::math::poisson_detail::check_dist(function,
            inDist.mean(), &outResult, Policy()))
            return kResultIsReady;
        else if (inP == 1) {
            outResult = boost::math::policies::raise_overflow_error<RealType>(
                function, 0, Policy());
            return kResultIsReady;
        }

        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};

/**
 * @brief Due to boost bug XXX, we need to override the domain check for
 *     quantile
 *
 * FIXME: No boost bug filed so far
 * Boost does not catch the case where sigma is NaN.
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::rayleigh_distribution<RealType, Policy> >
  : public PositiveDomainCheck<
        boost::math::rayleigh_distribution<RealType, Policy>
    > {
    typedef boost::math::rayleigh_distribution<RealType, Policy> Distribution;
    typedef PositiveDomainCheck<Distribution> Base;

    static bool check_sigma_finite(const char* function, RealType sigma,
        RealType* result, const Policy& pol) {

        if (!boost::math::isfinite(sigma)) {
            *result = boost::math::policies::raise_domain_error<RealType>(
                function,
                "The scale parameter \"sigma\" must be finite, but was: %1%.",
                sigma, pol);
            return false;
        }
        return true;
    }

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<rayleigh_distribution<%1%> >::cdf(...)";

        if (!check_sigma_finite(function, inDist.sigma(), &outResult, Policy()))
            return kResultIsReady;

        return Base::template cdf<Complement>(inDist, inX, outResult);
    }

    static ProbFnOverride pdf(const Distribution& inDist,
        const RealType& inX, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<rayleigh_distribution<%1%> >::pdf(...)";

        if (!check_sigma_finite(function, inDist.sigma(), &outResult, Policy()))
            return kResultIsReady;

        return Base::pdf(inDist, inX, outResult);
    }

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<rayleigh_distribution<%1%> >::quantile(...)";

        if (!check_sigma_finite(function, inDist.sigma(), &outResult, Policy()))
            return kResultIsReady;

        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};

/**
 * @brief Due to boost bugs 6938 and 6939, we need to override the domain checks
 *
 * https://svn.boost.org/trac/boost/ticket/6938
 * https://svn.boost.org/trac/boost/ticket/6939
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::weibull_distribution<RealType, Policy> >
  : public PositiveDomainCheck<
        boost::math::weibull_distribution<RealType, Policy>
    > {
    typedef boost::math::weibull_distribution<RealType, Policy> Distribution;
    typedef PositiveDomainCheck<Distribution> Base;

    // BEGIN Copied from boost/math/distributions/weibull.hpp (v1.49), but replaced
    // "<" check by "<=".
    static bool check_weibull_shape(
        const char* function,
        RealType shape,
        RealType* result, const Policy& pol) {

        if((shape <= 0) || !(boost::math::isfinite)(shape)) {
            *result = boost::math::policies::raise_domain_error<RealType>(
                function,
                "Shape parameter is %1%, but must be > 0 !", shape, pol);
            return false;
        }
        return true;
    }
    // END Copied from boost/math/distributions/weibull.hpp (v1.49)

    template <bool Complement>
    static ProbFnOverride cdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<weibull_distribution<%1%> >::cdf(...)";
        RealType shape = inDist.shape();
        if (!check_weibull_shape(function, shape, &outResult, Policy()))
            return kResultIsReady;

        return Base::template cdf<Complement>(inDist, inX, outResult);
    }

    static ProbFnOverride pdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<weibull_distribution<%1%> >::pdf(...)";
        RealType shape = inDist.shape();
        if (!check_weibull_shape(function, shape, &outResult, Policy()))
            return kResultIsReady;

        if (inX == 0) {
            if (shape == 1)
                outResult = 1;
            else if (shape < 1)
                outResult = boost::math::policies::raise_overflow_error<RealType>(
                    function, 0, Policy());
            else
                return kLetBoostCalculate;

            return kResultIsReady;
        }
        return Base::pdf(inDist, inX, outResult);
    }

    template <bool Complement>
    static ProbFnOverride quantile(const Distribution& inDist,
        const RealType& inP, RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<weibull_distribution<%1%> >::quantile(...)";
        RealType shape = inDist.shape();
        if (!check_weibull_shape(function, shape, &outResult, Policy()))
            return kResultIsReady;

        return Base::template quantile<Complement>(inDist, inP, outResult);
    }
};



#define DOMAIN_CHECK_OVERRIDE(dist, check) \
    template <> \
    template <class RealType, class Policy> \
    struct DomainCheck<boost::math::dist ## _distribution<RealType, Policy> > \
      : public check<boost::math::dist ## _distribution<RealType, Policy> > { };

DOMAIN_CHECK_OVERRIDE(beta, ZeroOneDomainCheck)
DOMAIN_CHECK_OVERRIDE(chi_squared, PositiveDomainCheck)
DOMAIN_CHECK_OVERRIDE(laplace, RealDomainCheck)
DOMAIN_CHECK_OVERRIDE(non_central_t, RealDomainCheck)
DOMAIN_CHECK_OVERRIDE(triangular, RealDomainCheck)
DOMAIN_CHECK_OVERRIDE(uniform, RealDomainCheck)
// The following lines are currently commented out. Each of these is a boost
// deficiency unfortunately. See above.
// DOMAIN_CHECK_OVERRIDE(exponential, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(extreme_value, RealDomainCheck)
// DOMAIN_CHECK_OVERRIDE(fisher_f, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(gamma, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(geometric, NonNegativeIntegerDomainCheck)
// DOMAIN_CHECK_OVERRIDE(lognormal, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(hypergeometric, NonNegativeIntegerDomainCheck)
// DOMAIN_CHECK_OVERRIDE(inverse_gamma, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(negative_binomial, NonNegativeIntegerDomainCheck)
// DOMAIN_CHECK_OVERRIDE(non_central_beta, ZeroOneDomainCheck)
// DOMAIN_CHECK_OVERRIDE(non_central_chi_squared, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(non_central_f, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(pareto, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(poisson, NonNegativeIntegerDomainCheck)
// DOMAIN_CHECK_OVERRIDE(rayleigh, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(weibull, PositiveDomainCheck)

#undef DOMAIN_CHECK_OVERRIDE

} // anonymous namespace

#define DEFINE_BOOST_WRAPPER(_dist, _what, _domain_check_what) \
    template <class RealType, class Policy> \
    inline \
    RealType \
    _what(const boost::math::_dist ## _distribution<RealType, Policy>& dist, \
        const RealType& x) { \
        \
        typedef boost::math::_dist ## _distribution<RealType, Policy> Dist; \
        RealType result; \
        switch (DomainCheck<Dist>::_domain_check_what(dist, x, result)) { \
            case kResultIsReady: return result; \
            case kLetBoostCalculate: return boost::math::_what(dist, x); \
            case kLetBoostCalculateUsingValue: \
                return boost::math::_what(dist, result); \
            default: throw std::logic_error("Unexpected case detected in " \
                "domain-check override for a boost probability function."); \
        } \
    }

#define DEFINE_BOOST_COMPLEMENT_WRAPPER(_dist, _what) \
    template <class RealType, class Policy> \
    inline \
    RealType \
    _what(const boost::math::complemented2_type< \
        boost::math::_dist ## _distribution<RealType, Policy>, \
        RealType \
    >& c) { \
        typedef boost::math::_dist ## _distribution<RealType, Policy> Dist; \
        typedef boost::math::complemented2_type<Dist, RealType> Complement; \
        RealType result; \
        switch (DomainCheck<Dist>::template _what<true>(c.dist, c.param, result)) { \
            case kResultIsReady: return result; \
            case kLetBoostCalculate: return boost::math::_what(c); \
            case kLetBoostCalculateUsingValue: \
                return boost::math::_what(Complement(c.dist, result)); \
            default: throw std::logic_error("Unexpected case detected in " \
                "domain-check override for a boost probability function."); \
        } \
    }

#define DEFINE_BOOST_PROBABILITY_DISTR(_dist) \
    typedef boost::math::_dist ## _distribution< \
        double, boost_mathkit_policy> _dist; \
    \
    DEFINE_BOOST_WRAPPER(_dist, cdf, template cdf<false>) \
    DEFINE_BOOST_COMPLEMENT_WRAPPER(_dist, cdf) \
    DEFINE_BOOST_WRAPPER(_dist, pdf, pdf) \
    DEFINE_BOOST_WRAPPER(_dist, quantile, template quantile<false>) \
    DEFINE_BOOST_COMPLEMENT_WRAPPER(_dist, quantile)


#define MADLIB_ITEM(_dist) \
    DEFINE_BOOST_PROBABILITY_DISTR(_dist)

// Note that boost also uses the pdf() if actually a probability mass function
// is meant
LIST_CONTINUOUS_PROB_DISTR
LIST_DISCRETE_PROB_DISTR

#undef MADLIB_ITEM
#undef DEFINE_PROBABILITY_DISTR
#undef DEFINE_BOOST_WRAPPER
#undef LIST_CONTINUOUS_PROB_DISTR

} // namespace prob

} // namespace modules

} // namespace regress

#endif // defined(MADLIB_MODULES_PROB_BOOST_HPP)
