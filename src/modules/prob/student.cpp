/* ----------------------------------------------------------------------- *//** 
 *
 * @file student.cpp
 *
 * @brief Evaluate the Student's-T distribution function using the boost library.
 *
 *//* -------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

// The error function is in C99 and TR1, but not in the official C++ Standard
// (before C++11). We therefore use the Boost implementation
#include <boost/math/special_functions/erf.hpp>


namespace madlib {
  namespace modules {
	namespace prob {

	  /**
	   * @brief Student's-t cumulative distribution function: In-database interface
	   * 
	   * Compute \f$ Pr[T <= x] \f$ for Student-t distributed T with \f$ \nu \f$
	   * degrees of freedom.
	   *
	   * Note: Implemented using the boost library.
	   */
	  AnyType students_t_cdf(AbstractDBInterface & /* db */, AnyType args) {
		AnyType::iterator   arg(args);
		const double        x  = *arg++;
		const double        df = *arg;

namespace modules {

namespace prob {

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#undef _DOXYGEN_IGNORE_HEADER_FILE
#include "student.hpp"

namespace {
    // Anonymous namespace for internal functions
    
    inline double normal_cdf(double t);
    double studentT_cdf_approx(int64_t nu, double t);
}


/**
 * @brief Student-t cumulative distribution function: C++ interface
 * 
 * Compute \f$ Pr[T <= t] \f$ for Student-t distributed T with \f$ \nu \f$
 * degrees of freedom.
 *
 * For nu >= 1000000, we just use the normal distribution as an approximation.
 * For 1000000 >= nu >= 200, we use a simple approximation from [9].
 * We are much more cautious than usual here (it is folklore that the normal
 * distribution is a "good" estimate for Student-T if nu >= 30), but we can
 * afford the extra work as this function is not designed to be called from
 * inner loops. Performance should still be reasonably good, with at most ~100
 * iterations in any case (just one if nu >= 200).
 * 
 * For nu < 200, we use the series expansions 26.7.3 and 26.7.4 from [1] and
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
 * @param nu Degree of freedom (>= 1)
 * @param t Argument to cdf.
 *
 * Note: The running time of calculating the series is proportional to nu.
 * We therefore use the normal distribution as an approximation for large nu.
 * Another idea for handling this case can be found in reference [8].
 */

double studentT_CDF(int64_t nu, double t) {
	double		z,
				t_by_sqrt_nu;
	double		A, /* contains A(t|nu) */
				prod = 1.,
				sum = 1.;

	/* Handle extreme cases. See above. */
	 
	if (nu <= 0)
		return std::numeric_limits<double>::quiet_NaN();
    else if (t == std::numeric_limits<double>::infinity())
        return 1;
    else if (t == -std::numeric_limits<double>::infinity())
        return 0;
	else if (nu >= 1000000)
		return normal_cdf(t);
	else if (nu >= 200)
		return studentT_cdf_approx(nu, t);

	/* Handle main case (nu < 200) in the rest of the function. */

	z = 1. + t * t / nu;
	t_by_sqrt_nu = std::fabs(t) / std::sqrt(static_cast<double>(nu));
	
	if (nu == 1)
	{
		A = 2. / M_PI * std::atan(t_by_sqrt_nu);
	}
	else if (nu & 1) /* odd nu > 1 */
	{
		for (int j = 2; j <= nu - 3; j += 2)
		{
			prod = prod * j / ((j + 1) * z);
			sum = sum + prod;
		}
		A = 2 / M_PI * ( std::atan(t_by_sqrt_nu) + t_by_sqrt_nu / z * sum );
	}
	else /* even nu */
	{
		for (int j = 2; j <= nu - 2; j += 2)
		{
			prod = prod * (j - 1) / (j * z);
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
	
	/* The Student-T distribution is obviously symmetric around t=0... */
	if (t < 0)
		return .5 * (1. - A);
	else
		return 1. - .5 * (1. - A);
}

namespace {

/**
 * @brief Compute the normal distribution function using the library error function.
 *
 * This approximation satisfies
 * rel_error < 0.0001 || abs_error < 0.00000001
 * for all nu >= 1000000. (Tested on Mac OS X 10.6, gcc-4.2.)
 */

inline double normal_cdf(double t)
{
	return .5 + .5 * boost::math::erf(t / std::sqrt(2.));
}


/**
 * @brief Approximate Student-T distribution using a formula suggested in
 *        [9], which goes back to an approximation suggested in [10].
 *
 * Compared to the series expansion, this approximation satisfies
 * rel_error < 0.0001 || abs_error < 0.00000001
 * for all nu >= 200. (Tested on Mac OS X 10.6, gcc-4.2.)
 */
double studentT_cdf_approx(int64_t nu, double t)
{
	double	g = (nu - 1.5) / ((nu - 1) * (nu - 1)),
			z = std::sqrt( std::log(1. + t * t / nu) / g );

	if (t < 0)
		z *= -1.;
	
	return normal_cdf(z);
}

}

/**
 * @brief Student-t cumulative distribution function: In-database interface
 */
AnyType
student_t_cdf::run(AnyType &args) {
    int64_t nu = args[0].getAs<int64_t>();
    double t = args[1].getAs<double>();
        
    /* We want to ensure nu > 0 */
    if (nu <= 0)
        throw std::domain_error("Student-t distribution undefined for "
            "degree of freedom <= 0");

    return studentT_CDF(nu, t);
}

} // namespace prob

} // namespace modules

} // namespace regress
