/* ----------------------------------------------------------------------- *//**
 *
 * @file kolmogorov.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Kolmogorov cumulative distribution function
 */
DECLARE_UDF(prob, kolmogorov_cdf)


#ifndef MADLIB_MODULES_PROB_KOLMOGOROV_HPP
#define MADLIB_MODULES_PROB_KOLMOGOROV_HPP

#include <boost/math/policies/policy.hpp>
#include <boost/math/distributions/complement.hpp>
#include <boost/math/distributions/detail/common_error_handling.hpp>

namespace madlib {

namespace modules {

namespace prob {

// CERN ROOT namespace. We use this namespace, so that the original
// implementation could be copied as is, without any modification.
namespace TMath {
    // CERN ROOT type definitions in: core/base/inc/Rtypes.h
    typedef int32_t Int_t;
    typedef double Double_t;
    Double_t KolmogorovProb(Double_t z);
}

template <
    class RealType = double,
    class Policy = boost::math::policies::policy<>
>
class kolmogorov_distribution {
public:
    typedef RealType value_type;
    typedef Policy policy_type;

    kolmogorov_distribution() { }
};

typedef kolmogorov_distribution<double, boost_mathkit_policy> kolmogorov;

/**
 * @brief Range of permissible values for random variable x.
 */
template <class RealType, class Policy>
inline
const std::pair<RealType, RealType>
range(const kolmogorov_distribution<RealType, Policy>& /*dist*/) {
    return std::pair<RealType, RealType>(
        static_cast<RealType>(0), static_cast<RealType>(1));
}

/**
 * @brief Range of supported values for random variable x.
 *
 * This is range where cdf rises from 0 to 1, and outside it, the pdf is zero.
 */
template <class RealType, class Policy>
inline
const std::pair<RealType, RealType>
support(const kolmogorov_distribution<RealType, Policy>& /*dist*/) {
    return std::pair<RealType, RealType>(
        static_cast<RealType>(0), static_cast<RealType>(1));
}

template <class RealType, class Policy>
inline
RealType
cdf(const kolmogorov_distribution<RealType, Policy>&, const RealType& x) {
    static const char* function = "madlib::modules::prob::cdf("
        "const kolmogorov_distribution<%1%>&, %1%)";
    RealType result;

    if ((boost::math::isinf)(x)) {
        if(x < 0) return 0; // -infinity
        return 1; // + infinity
    }
    if (boost::math::detail::check_x(function, x, &result, Policy()) == false)
        return result;

    // FIXME: Numerically questionable if t is close to 1.
    return 1. - TMath::KolmogorovProb(x);
}

template <class RealType, class Policy>
inline
RealType
cdf(
    const boost::math::complemented2_type<
        kolmogorov_distribution<RealType, Policy>,
        RealType
    >& c
) {
    static const char* function = "madlib::modules::prob::cdf("
        "const complement(kolmogorov_distribution<%1%>&), %1%)";
    RealType result;
    const RealType& x = c.param;

    if ((boost::math::isinf)(x)) {
        if(x < 0) return 1; // cdf complement -infinity is unity.
        return 0; // cdf complement +infinity is zero
    }
    if (boost::math::detail::check_x(function, x, &result, Policy()) == false)
        return result;

    return TMath::KolmogorovProb(x);
}

} // namespace prob

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_PROB_KOLMOGOROV_HPP)
