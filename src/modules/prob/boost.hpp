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


#define LIST_DISCRETE_PROB_DISTR \
    MADLIB_ITEM(bernoulli) \
    MADLIB_ITEM(binomial) \
    MADLIB_ITEM(geometric) \
    MADLIB_ITEM(hypergeometric) \
    MADLIB_ITEM(negative_binomial)


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
    typedef typename Distribution::value_type value_type;

    static bool cdf(const value_type& /* inX */, value_type& /* outResult */) {
        return true;
    }
    
    static bool pdf(const value_type&, value_type&) {
        return true;
    }
    
    static bool quantile(const value_type&, value_type&) {
        return true;
    }
};

/**
 * @brief Domain-check overrides for Fisher F-distribution function
 */
template <>
template <class RealType, class Policy>
struct DomainCheck<boost::math::fisher_f_distribution<RealType, Policy> > {
    typedef RealType value_type;

    static bool cdf(const value_type& inX, value_type& outResult) {
        if (inX < 0)
            outResult = 0;
        else if (!std::isfinite(inX))
            outResult = 1;
        else
            return true;
        
        return false;
    }
    
    static bool pdf(const value_type& inX, value_type& outResult) {
        if (inX < 0 || !std::isfinite(inX))
            outResult = 0;
        else
            return true;
        
        return false;
    }
    
    static bool quantile(const value_type&, value_type&) {
        return true;
    }
};

} // anonymous namespace


#define DEFINE_BOOST_WRAPPER(dist, what) \
    template <class RealType, class Policy> \
    inline \
    RealType \
    what(const boost::math::dist ## _distribution<RealType, Policy>& dist, \
        const RealType& t) { \
        \
        typedef boost::math::dist ## _distribution<RealType, Policy> Dist; \
        RealType result; \
        return DomainCheck<Dist>::what(t, result) \
            ? boost::math::what(dist, t) \
            : result; \
    } \
    \
    template <class RealType, class Policy> \
    inline \
    RealType \
    what(const boost::math::complemented2_type< \
        boost::math::dist ## _distribution<RealType, Policy>, \
        RealType \
    >& c) { \
        typedef boost::math::dist ## _distribution<RealType, Policy> Dist; \
        RealType result; \
        return DomainCheck<Dist>::what(c.param, result) \
            ? boost::math::what(c) \
            : result; \
    }

#define DEFINE_BOOST_PROBABILITY_DISTR(dist) \
    typedef boost::math::dist ## _distribution< \
        double, boost_mathkit_policy> dist; \
    \
    DEFINE_BOOST_WRAPPER(dist, cdf) \
    DEFINE_BOOST_WRAPPER(dist, pdf) \
    DEFINE_BOOST_WRAPPER(dist, quantile)


#define MADLIB_ITEM(dist) \
    DEFINE_BOOST_PROBABILITY_DISTR(dist)

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
