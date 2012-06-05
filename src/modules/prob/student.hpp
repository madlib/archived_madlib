/* ----------------------------------------------------------------------- *//**
 *
 * @file student.hpp
 *
 * @brief Evaluate the Student's t-distribution function.
 * @author Florian Schoppmann
 * @date   November 2010
 *
 *//* -------------------------------------------------------------------- *//**
 *
 * @file student.hpp
 *
 * Emprirical results indicate that the numerical quality of the series
 * expansion from [1] (see notes below) is vastly superior to using continued
 * fractions for computing the cdf via the incomplete beta function.
 *
 * @literature
 *
 * [1] Abramowitz and Stegun, Handbook of Mathematical Functions with Formulas,
 *     Graphs, and Mathematical Tables, 1972
 *     page 948: http://people.math.sfu.ca/~cbm/aands/page_948.htm
 *
 * Further reading (for computing the Student-T cdf via the incomplete beta
 * function):
 *
 * [2] NIST Digital Library of Mathematical Functions, Ch. 8,
 *     Incomplete Gamma and Related Functions,
 *     http://dlmf.nist.gov/8.17
 *
 * [3] Lentz, Generating Bessel functions in Mie scattering calculations using
 *     continued fractions, Applied Optics, Vol. 15, No. 3, 1976
 *
 * [4] Thompson and Barnett, Coulomb and Bessel Functions of Complex Arguments
 *     and Order, Journal of Computational Physics, Vol. 64, 1986
 *
 * [5] Cuyt et al., Handbook of Continued Fractions for Special Functions,
 *     Springer, 2008
 *
 * [6] Gil et al., Numerical Methods for Special Functions, SIAM, 2008
 *
 * [7] Press et al., Numerical Recipes in C++, 3rd edition,
 *     Cambridge Univ. Press, 2007
 *
 * [8] DiDonato, Morris, Jr., Algorithm 708: Significant Digit Computation of
 *     the Incomplete Beta Function Ratios, ACM Transactions on Mathematical
 *     Software, Vol. 18, No. 3, 1992
 *
 * Approximating the Student-T distribution function with the normal
 * distribution:
 *
 * [9]  Gleason, A note on a proposed student t approximation, Computational
 *      Statistics & Data Analysis, Vol. 34, No. 1, 2000
 *
 * [10] Gaver and Kafadar, A Retrievable Recipe for Inverse t, The American
 *      Statistician, Vol. 38, No. 4, 1984
 */

/**
 * @brief Student-t cumulative distribution function
 */
DECLARE_UDF(prob, students_t_cdf)
DECLARE_UDF(prob, students_t_pdf)
DECLARE_UDF(prob, students_t_quantile)


#ifndef MADLIB_MODULES_PROB_STUDENT_T_HPP
#define MADLIB_MODULES_PROB_STUDENT_T_HPP

#include <boost/math/distributions/detail/common_error_handling.hpp>
#include <boost/math/distributions/normal.hpp>
#include <boost/math/distributions/students_t.hpp>

namespace madlib {

namespace modules {

namespace prob {

typedef boost::math::students_t_distribution<double, boost_mathkit_policy>
    students_t;

namespace {

/**
 * @brief Compute one-sided Student's t cumulative distribution function
 *
 * We use the series expansions 26.7.3 and 26.7.4 from [1] and
 * substitute sin(theta) = t/sqrt(n * z), where z = 1 + t^2/nu.
 *
 * This gives:
 * @verbatim
 *                          t
 *   A(t|1)  = 2 arctan( -------- ) ,
 *                       sqrt(nu)
 *
 *                                                    (nu-3)/2
 *             2   [            t              t         --    2 * 4 * ... * (2i)  ]
 *   A(t|nu) = - * [ arctan( -------- ) + ------------ * \  ---------------------- ]
 *             π   [         sqrt(nu)     sqrt(nu) * z   /_ 3 * ... * (2i+1) * z^i ]
 *                                                       i=0
 *           for odd nu > 1, and
 *
 *                         (nu-2)/2
 *                  t         -- 1 * 3 * ... * (2i - 1)
 *   A(t|nu) = ------------ * \  ------------------------ for even nu,
 *             sqrt(nu * z)   /_ 2 * 4 * ... * (2i) * z^i
 *                            i=0
 *
 * where A(t|nu) = Pr[|T| <= t].
 * @endverbatim
 *
 * @param t
 * @param nu Degree of freedom \f$ \nu > 0 \f$
 * @return \f$ \Pr[|T| < t] \f$ where \f$ t \geq 0 \f$, \f$ T \f$ is a Student's
 *     T-distributed random variable with \f$ \nu \f$ degrees of
 *     freedom.
 *
 * Note: The running time of calculating the series is proportional to nu.
 * We therefore use the normal distribution as an approximation for large nu.
 * Another idea for handling this case can be found in reference [8].
 */
template <class RealType>
inline
RealType
oneSidedStudentsT_CDF(const RealType& t,  uint64_t nu) {
    RealType    z,
                t_by_sqrt_nu;
    RealType    A, /* contains A(t|nu) */
                prod = 1.,
                sum = 1.;

    /* Handle main case (nu \in {1, ..., 200}) in the rest of the function. */
    z = 1. + t * t / static_cast<double>(nu);
    t_by_sqrt_nu = std::fabs(t) / std::sqrt(static_cast<double>(nu));

    if (nu == 1)
    {
        A = 2. / M_PI * std::atan(t_by_sqrt_nu);
    }
    else if (nu & 1) /* odd nu > 1 */
    {
        for (uint64_t j = 2; j + 3 <= nu; j += 2)
        {
            prod = prod * static_cast<double>(j)
                 / (static_cast<double>(j + 1) * z);
            sum = sum + prod;
        }
        A = 2 / M_PI * ( std::atan(t_by_sqrt_nu) + t_by_sqrt_nu / z * sum );
    }
    else /* even nu */
    {
        for (uint64_t j = 2; j + 2 <= nu; j += 2)
        {
            prod = prod * static_cast<double>(j - 1)
                 / (static_cast<double>(j) * z);
            sum = sum + prod;
        }
        A = t_by_sqrt_nu / std::sqrt(z) * sum;
    }

    /* A should obviously be within the interval [0,1] plus minus (hopefully
     * small) rounding errors. */
    if (A > 1.)
        A = 1.;
    else if (A < 0.)
        A = 0.;

    return A;
}

/**
 * @brief Compute parameter for normal CDF for approximating the Student's T CDF
 *
 * Gleason suggested a formula for approximating the Student's
 * t-distribution [9], which goes back to an approximation suggested in [10].
 *
 * Compared to the series expansion, this approximation satisfies
 * rel_error < 0.0001 || abs_error < 0.00000001
 * for all nu >= 200. (Tested on Mac OS X 10.6, gcc-4.2.)
 *
 * @param t
 * @param nu Degree of freedom \f$ \nu > 0 \f$
 * @returns A value \f$ z \f$ such that for a Student's t-distributed
 *     random variable \f$ T \f$ with \f$ nu \f$ degrees of freedom and a
 *     standard normally distributed random variable \f$ Z \f$, it holds that
 *     \f$ \Pr[T \leq t] \approx \Pr[Z \leq z] \f$.
 */
template <class RealType>
inline
RealType
GleasonsNormalApproxForStudentsT(const RealType& t, const RealType& nu) {
    double  g = (nu - 1.5) / ((nu - 1) * (nu - 1)),
            z = std::sqrt( std::log(1. + t * t / nu) / g );

    if (t < 0)
        z *= -1.;

    return z;
}

} // anonymous namespace

/**
 * @brief Compute Student's cumulative distribution function
 *
 * For nu >= 1000000, we just use the normal distribution as an approximation.
 * For 1000000 >= nu >= 200, we use a simple approximation from [9].
 * If nu is not within 0.01 of a natural number, we will call the student-t
 * CDF from boost. Otherwise, our approach should be much more precise than
 * using the incomplete beta function as boost does (see the references).
 *
 * We are much more cautious than usual here (it is folklore that the normal
 * distribution is a "good" estimate for Student-T if nu >= 30), but we can
 * afford the extra work as this function is not designed to be called from
 * inner loops. Performance should still be reasonably good, with at most ~100
 * iterations in any case (just one if nu >= 200).
 *
 * For nu < 200, we use the series expansions 26.7.3 and 26.7.4 from [1] and
 * substitute sin(theta) = t/sqrt(n * z), where z = 1 + t^2/nu (using
 * oneSidedStudentsT_CDF()).
 *
 * @param dist A Student's t-distribution object, containing the degree of
 *     freedom \f$ \nu \f$
 * @param t
 * @return \f$ \Pr[T < t] \f$ where \f$ t \geq 0 \f$, \f$ T \f$ is a Student's
 *     T-distributed random variable with \f$ \nu \f$ degrees of
 *     freedom.
 */
template <class RealType, class Policy>
inline
RealType
cdf(const boost::math::students_t_distribution<RealType, Policy>& dist,
    const RealType& t) {

    RealType df = dist.degrees_of_freedom();

    // FIXME: Add some justification/do some tests.
    if (!std::isfinite(df) || std::fabs(df - std::floor(df))/df > 0.01)
        return boost::math::cdf(dist, t);

    static const char* function = "madlib::modules::prob::cdf("
        "const students_t_distribution<%1%>&, %1%)";

    RealType result;
    if (!boost::math::detail::check_df(function, df, &result, Policy()))
        return result;

    if (df >= 200)
        return
            boost::math::cdf(
                boost::math::normal_distribution<RealType, Policy>(),
                df >= 1000000
                    ? t
                    : GleasonsNormalApproxForStudentsT(t, df)
            );

    // We first compute A = Pr[|T| < t]
    RealType A = oneSidedStudentsT_CDF(t, static_cast<uint64_t>(df));

    /* The Student-T distribution is obviously symmetric around t=0... */
    if (t < 0)
        /* FIXME: If A is approximately 1, we will face a loss of significance.
         *  */
        return .5 * (1. - A);
    else
        /* While we only know A in [0,1] here, the end result will be in
         * [0.5, 1]. Hence, there is no problem with adding 1 and A, even if
         * A << 1. */
        return .5 * (1. + A);
}

/**
 * @brief Compute the complement of Student's cumulative distribution function
 */
template <class RealType, class Policy>
inline
RealType
cdf(
    const boost::math::complemented2_type<
        boost::math::students_t_distribution<RealType, Policy>,
        RealType
    >& c
) {
    RealType df = c.dist.degrees_of_freedom();
    if (df >= 200) {
        static const char* function = "madlib::modules::prob::cdf("
            "const complement(students_t_distribution<%1%>&), %1%)";

        RealType result;
        if (!boost::math::detail::check_df(function, df, &result, Policy()))
            return result;

        return
            boost::math::cdf(complement(
                boost::math::normal_distribution<RealType, Policy>(),
                df >= 1000000
                    ? c.param
                    : GleasonsNormalApproxForStudentsT(c.param, df)
            ));
    }

    return prob::cdf(c.dist, -c.param);
}

template <class RealType, class Policy>
inline
RealType
pdf(const boost::math::students_t_distribution<RealType, Policy>& dist,
    const RealType& t) {
    return boost::math::pdf(dist, t);
}

template <class RealType, class Policy>
inline
RealType
pdf(
    const boost::math::complemented2_type<
        boost::math::students_t_distribution<RealType, Policy>,
        RealType
    >& c
) {
    return boost::math::pdf(c);
}

template <class RealType, class Policy>
inline
RealType
quantile(const boost::math::students_t_distribution<RealType, Policy>& dist,
    const RealType& p) {

    using namespace boost::math;

    static const char* function = "madlib::modules::prob::quantile("
        "const students_t_distribution<%1%>&, %1%)";

    // FIXME: Boost bug 6937 prevent proper argument validation.
    // https://svn.boost.org/trac/boost/ticket/6937
    // Until this is fixed upstream, we do the following checks here.
    RealType df = dist.degrees_of_freedom();
    RealType result;
    if (!detail::check_df(function, df, &result, Policy())
        || !detail::check_probability(function, p, &result, Policy()))
        return result;

    return boost::math::quantile(dist, p);
}

template <class RealType, class Policy>
inline
RealType
quantile(
    const boost::math::complemented2_type<
        boost::math::students_t_distribution<RealType, Policy>,
        RealType
    >& c
) {
    return boost::math::quantile(c);
}

} // namespace prob

} // namespace modules

} // namespace madlib

#endif // defined(MADLIB_MODULES_PROB_STUDENT_T_HPP)
