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
// FIXME: Pending Boost bug 6934, we currently do not support the
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


#if !defined(DECLARE_LIBRARY_EXPORTS)

#include <boost/math/distributions.hpp>

namespace madlib {

namespace modules {

namespace prob {

namespace {
// No need to make this visiable beyond this translation unit.

/**
 * @brief Via (partial) spezialization, this class offers a way to override
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
    static bool cdf(const Distribution&, const RealType&, RealType&) {
        return true;
    }

    static bool pdf(const Distribution&, const RealType&, RealType&) {
        return true;
    }

    template <bool Complement>
    static bool quantile(const Distribution&, const RealType&, RealType&) {
        return true;
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
    static bool cdf(const Distribution&, const RealType& inX,
        RealType& outResult) {

        if (boost::math::isinf(inX)) {
            if (inX < 0)
                outResult = Complement ? 1 : 0;
            else
                outResult = Complement ? 0 : 1;
            return false;
        }
        return true;
    }

    static bool pdf(const Distribution&, const RealType& inX,
        RealType& outResult) {

        if (boost::math::isinf(inX)) {
            outResult = 0;
            return false;
        }
        return true;
    }

    template <bool Complement>
    static bool quantile(const Distribution&, const RealType&, RealType&) {
        return true;
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
    static bool cdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        if (inX < 0) {
            outResult = Complement ? 1 : 0;
            return false;
        }
        return Base::template cdf<Complement>(inDist, inX, outResult);
    }

    static bool pdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        if (inX < 0) {
            outResult = 0;
            return false;
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
    static bool cdf(const Distribution&, const RealType& inX,
        RealType& outResult) {

        if (inX < 0)
            outResult = Complement ? 1 : 0;
        else if (inX > 1)
            outResult = Complement ? 0 : 1;
        else
            return true;

        return false;
    }

    static bool pdf(const Distribution&, const RealType& inX,
        RealType& outResult) {

        if (inX < 0 || inX > 1) {
            outResult = 0;
            return false;
        }
        return true;
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
    static bool quantile(const Distribution& inDist, const RealType& inP,
        RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<fisher_f_distribution<%1%> >::quantile(...)";

        if (std::isnan(inP)) {
            outResult = boost::math::policies::raise_domain_error<RealType>(
                function,
                "Probability argument is %1%, but must be >= 0 and <= 1!", inP,
                Policy());
            return false;
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

    static bool pdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<gamma_distribution<%1%> >::pdf(...)";
        RealType shape = inDist.shape();
        RealType scale = inDist.scale();
        if (!boost::math::detail::check_gamma(function, scale, shape,
            &outResult, Policy()))
            return false;

        if (inX == 0) {
            if (shape == 1)
                outResult = 1. / scale;
            else if (shape < 1)
                outResult
                    = boost::math::policies::raise_overflow_error<RealType>(
                        function, 0, Policy());
            else
                return true;

            return false;
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
    static bool quantile(const Distribution& inDist, const RealType& inP,
        RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<pareto_distribution<%1%> >::quantile(...)";

        if (!boost::math::detail::check_pareto(function, inDist.scale(),
            inDist.shape(), &outResult, Policy()))
            return false;
        else if (inP == 1) {
            outResult = boost::math::policies::raise_overflow_error<RealType>(
                function, 0, Policy());
            return false;
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

    template <bool Complement>
    static bool quantile(const Distribution& inDist, const RealType& inP,
        RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<rayleigh_distribution<%1%> >::quantile(...)";

        RealType sigma = inDist.sigma();
        if (!boost::math::isfinite(sigma)) {
            outResult = boost::math::policies::raise_domain_error<RealType>(
                function,
                "The scale parameter \"sigma\" must be > 0, but was: %1%.",
                sigma,
                Policy());
            return false;
        }
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
    static bool cdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<weibull_distribution<%1%> >::cdf(...)";
        RealType shape = inDist.shape();
        if (!check_weibull_shape(function, shape, &outResult, Policy()))
            return false;

        return Base::template cdf<Complement>(inDist, inX, outResult);
    }

    static bool pdf(const Distribution& inDist, const RealType& inX,
        RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<weibull_distribution<%1%> >::pdf(...)";
        RealType shape = inDist.shape();
        if (!check_weibull_shape(function, shape, &outResult, Policy()))
            return false;

        if (inX == 0) {
            if (shape == 1)
                outResult = 1;
            else if (shape < 1)
                outResult = boost::math::policies::raise_overflow_error<RealType>(
                    function, 0, Policy());
            else
                return true;

            return false;
        }
        return Base::pdf(inDist, inX, outResult);
    }

    template <bool Complement>
    static bool quantile(const Distribution& inDist, const RealType& inP,
        RealType& outResult) {

        static const char* function = "madlib::modules::prob::<unnamed>::"
            "DomainCheck<weibull_distribution<%1%> >::quantile(...)";
        RealType shape = inDist.shape();
        if (!check_weibull_shape(function, shape, &outResult, Policy()))
            return false;

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
// The following lines are currently commented out. See above.
// DOMAIN_CHECK_OVERRIDE(fisher_f, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(gamma, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(pareto, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(rayleigh, PositiveDomainCheck)
// DOMAIN_CHECK_OVERRIDE(weibull, PositiveDomainCheck)

#undef DOMAIN_CHECK_OVERRIDE

} // anonymous namespace

#define DEFINE_BOOST_WRAPPER(_dist, _what, _domain_check_what) \
    template <class RealType, class Policy> \
    inline \
    RealType \
    _what(const boost::math::_dist ## _distribution<RealType, Policy>& _dist, \
        const RealType& t) { \
        \
        typedef boost::math::_dist ## _distribution<RealType, Policy> Dist; \
        RealType result; \
        return DomainCheck<Dist>::_domain_check_what(_dist, t, result) \
            ? boost::math::_what(_dist, t) \
            : result; \
    } \

#define DEFINE_BOOST_COMPLEMENT_WRAPPER(_dist, _what) \
    template <class RealType, class Policy> \
    inline \
    RealType \
    _what(const boost::math::complemented2_type< \
        boost::math::_dist ## _distribution<RealType, Policy>, \
        RealType \
    >& c) { \
        typedef boost::math::_dist ## _distribution<RealType, Policy> Dist; \
        RealType result; \
        return DomainCheck<Dist>::template _what<true>(c.dist, c.param, result) \
            ? boost::math::_what(c) \
            : result; \
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

#endif // !defined(DECLARE_LIBRARY_EXPORTS)
