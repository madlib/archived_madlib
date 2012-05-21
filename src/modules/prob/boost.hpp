/* ----------------------------------------------------------------------- *//**
 *
 * @file boost.hpp
 *
 *//* ----------------------------------------------------------------------- */

#define LIST_CONTINUOUS_PROB_DISTR \
    MADLIB_ITEM(beta) \
    MADLIB_ITEM(chi_squared) \
    MADLIB_ITEM(fisher_f) \
    MADLIB_ITEM(exponential) \
    MADLIB_ITEM(gamma) \
    MADLIB_ITEM(normal)

#define MADLIB_ITEM(dist) \
    DECLARE_UDF(prob, dist ## _cdf) \
    DECLARE_UDF(prob, dist ## _pdf) \
    DECLARE_UDF(prob, dist ## _quantile)

LIST_CONTINUOUS_PROB_DISTR

#undef MADLIB_ITEM


#if !defined(DECLARE_LIBRARY_EXPORTS)

#include <boost/math/distributions/beta.hpp>
#include <boost/math/distributions/chi_squared.hpp>
#include <boost/math/distributions/exponential.hpp>
#include <boost/math/distributions/fisher_f.hpp>
#include <boost/math/distributions/gamma.hpp>
#include <boost/math/distributions/normal.hpp>

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

#define DEFINE_BOOST_WRAPPERS(dist) \
    DEFINE_BOOST_WRAPPER(dist, cdf) \
    DEFINE_BOOST_WRAPPER(dist, pdf) \
    DEFINE_BOOST_WRAPPER(dist, quantile)

#define MADLIB_ITEM(dist) \
    typedef boost::math::dist ## _distribution< \
        double, boost_mathkit_policy> dist; \
    \
    DEFINE_BOOST_WRAPPER(dist, cdf) \
    DEFINE_BOOST_WRAPPER(dist, pdf) \
    DEFINE_BOOST_WRAPPER(dist, quantile)

LIST_CONTINUOUS_PROB_DISTR

#undef MADLIB_ITEM
#undef DEFINE_BOOST_WRAPPERS
#undef DEFINE_BOOST_WRAPPER
#undef LIST_CONTINUOUS_PROB_DISTR

} // namespace prob

} // namespace modules

} // namespace regress

#endif // !defined(DECLARE_LIBRARY_EXPORTS)
