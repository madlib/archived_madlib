/* -----------------------------------------------------------------------------
 *
 * student.c
 *
 * Evaluate the Student-T distribution function.
 *
 * -----------------------------------------------------------------------------
 *
 * Author: Florian Schoppmann
 * Date:   November 2010
 * 
 * This file is compliant with the C99 standard (Compile with "-std=c99").
 *
 * Emprirical results indicate that the numerical quality of the series
 * expansion from [1] (see notes below) is vastly superior to using continued
 * fractions for computing the cdf via the incomplete beta function.
 *
 * Main reference:
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
 * [3] Lentz, Generating Bessel functions in Mie scattering calculations using
 *     continued fractions, Applied Optics, Vol. 15, No. 3, 1976
 * [4] Thompson and Barnett, Coulomb and Bessel Functions of Complex Arguments
 *     and Order, Journal of Computational Physics, Vol. 64, 1986
 * [5] Cuyt et al., Handbook of Continued Fractions for Special Functions,
 *     Springer, 2008
 * [6] Gil et al., Numerical Methods for Special Functions, SIAM, 2008
 * [7] Press et al., Numerical Recipes in C++, 3rd edition,
 *     Cambridge Univ. Press, 2007
 * [8] DiDonato, Morris, Jr., Algorithm 708: Significant Digit Computation of
 *     the Incomplete Beta Function Ratios, ACM Transactions on Mathematical
 *     Software, Vol. 18, No. 3, 1992
 */
 
#include "student.h"

#include <math.h>

/* 
 * double studentT_cdf(unsigned long nu, double t)
 * -----------------------------------------------
 *
 * Compute Pr[T <= t] for Student-t distributed T with nu degrees of freedom.
 *
 * We use the series expansions 26.7.3 and 26.7.4 from [1] and substitute
 * sin(theta) = t/sqrt(n * z), where z = 1 + t^2/nu.
 *
 * This gives:
 *                          t
 *   A(t|1)  = 2 arctan( -------- ) ,
 *                       sqrt(nu)
 *
 *                                                    (nu-3)/2
 *             2   [            t              t         --    2 * 4 * ... * (2i)  ]
 *   A(t|nu) = - * [ arctan( -------- ) + ------------ * \  ---------------------- ]
 *			   π   [         sqrt(nu)     sqrt(nu) * z   /_ 3 * ... * (2i+1) * z^i ]
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
 *
 * Note: The running time of this function is proportional to nu. This might not
 * be acceptable for large nu (e.g., if nu >> 1000). But in this case,
 * approximating the Student-T distribution with the normal distribution should
 * be sufficient for all practical matters anyway. If needed, reference [8]
 * could be a valuable source for handling the case nu >> 1000.
 */

double studentT_cdf(unsigned long nu, double t) {
	double		z = 1. + t * t / nu,
				t_by_sqrt_nu = fabs(t) / sqrt(nu);
	double		A, /* contains A(t|nu) */
				prod = 1.,
				sum = 1.;
	
	if (nu == 1)
	{
		A = 2. / M_PI * atan(t_by_sqrt_nu);
	}
	else if (nu & 1) /* odd nu > 1 */
	{
		for (int j = 2; j <= nu - 3; j += 2)
		{
			prod = prod * j / ((j + 1) * z);
			sum = sum + prod;
		}
		A = 2 / M_PI * ( atan(t_by_sqrt_nu) + t_by_sqrt_nu / z * sum );
	}
	else /* even nu */
	{
		for (int j = 2; j <= nu - 2; j += 2)
		{
			prod = prod * (j - 1) / (j * z);
			sum = sum + prod;
		}
		A = t_by_sqrt_nu / sqrt(z) * sum;
	}
	
	/* A should obviously lie withing the interval [0,1] plus minus (hopefully
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
