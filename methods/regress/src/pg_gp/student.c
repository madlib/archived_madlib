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
 * Note: Unless _REENTRANT is defined, this code is, strictly speaking, NOT
 * thread-safe because of its calling of lgamma. If _REENTRANT is defined, the
 * thread-safe (but technically non-standard) lgamma_r will be used. As of 
 * November 2010, lgamma_r should exist on most platforms.
 */
 
#include "student.h"

#include <math.h>

/*
 * double lgamma_internal(double x)
 * --------------------------------
 *
 * Special-purpose function to compute the logarithm of Gamma(x) when x >= 0.
 */
 
static double lgamma_internal(double x)
{	
#ifdef _REENTRANT
	/* sign is ignored and only needed for the call to lgamma */
	int			sign;

	return lgamma_r(x, &sign);
#else
	/* Note: lgamma is not thread-safe! See man page. */
	return lgamma(x);
#endif
}

/*
 * double incompleteBeta_internal(double a, double b, double x)
 * ------------------------------------------------------------
 *
 * Special-purpose function to compute the incomplete beta function I_x(a,b)
 * when min(a,b) <= 1 and x <= 1/2.
 *
 * We use the following well-known continued-fraction representation,
 * which is well-suited for numerical computation:
 *
 *              x^a (1 - x)^b     Gamma(a + b)      [ 1  d_1 d_2     ]
 *   I_x(a,b) = ------------- * ----------------- * [ -- --- --- ... ]
 *                    a         Gamma(a) Gamma(b)   [ 1+  1+  1+     ]
 *
 * where
 *
 *   d_1 = 1,
 *
 *                 i (b-i) x                           (a + i)(a + b + i) x
 *   d_{2i} = --------------------, and d_{2i + 1} = - --------------------
 *            (a + 2i - 1)(a + 2i)                     (a + 2i)(a + 2i + 1)
 *
 * Further reading:
 * [1] NIST Digital Library of Mathematical Functions, Ch. 8,
 *     Incomplete Gamma and Related Functions,
 *     http://dlmf.nist.gov/8.17
 * [2] Lentz, Generating Bessel functions in Mie scattering calculations using
 *     continued fractions, Applied Optics, Vol. 15, No. 3, 1976
 * [3] Thompson and Barnett, Coulomb and Bessel Functions of Complex Arguments
 *     and Order, Journal of Computational Physics, Vol. 64, 1986
 * [4] Cuyt et al., Handbook of Continued Fractions for Special Functions,
 *     Springer, 2008
 * [5] Gil et al., Numerical Methods for Special Functions, SIAM, 2008
 * [6] Press et al., Numerical Recipes in C++, 3rd edition,
 *     Cambridge Univ. Press, 2007
 */

static double incompleteBeta_internal(double a, double b, double x)
{	
	const int MAX_ITERATIONS = 100;
	
	/* PRECISION = precision of IEEE 754 double precision (80 bit), which is
	 * PRECISION = 2^{-52}. This is the ratio of two consecutive numbers in
	 * double precision floating-point arithmetic. */
	const double PRECISION = 0x1p-52;
	
	/* EPSILON = PRECISION^2. The assumption is that d / EPSILON is always
	 * greater than the floating-point resolution  2^52 (but yet causes no 
	 * exponent overflow), so that adding 1 has no consequence (see below). */
	const double EPSILON = 0x1p-104;	
		
	/* product = product of factors in front of continued fraction  */
	double		product = pow(x,a) * pow(1. - x, b) / a 
						* exp(lgamma_internal(a + b)
							  - lgamma_internal(a) - lgamma_internal(b));
		
	/*
	 * The i-th approximant of a continued fraction
	 * b_0 + (a_1 / (b_1 + a_2 / (...))) is
	 *
	 *   f_i = A_i/B_i
	 *
	 * where A_i = A_{i-1} b_i + A_{i-2} a_i
	 * and   B_i = B_{i-1} b_i + B_{i-2} a_i
	 * and, in our case, b_i = 1 and a_1 = 1 and a_i = d_{i-1}.
	 * Here, A_{-1} = 1, A_0 = 0, B_{-1} = 0, B_0 = 1
	 * 
	 * A well-known technique in the literature is not to compute these 
	 * three-term linear recurrences but to compute
	 * 
	 *   P_i = A_i / A_{i-1} and Q_i = B_i / B_{i-1}
	 *
	 * where then
	 *
	 *   f_i = f_{i-1} P_i / Q_i
	 *
	 * From the three-term linear recurrences, we know
	 *
	 *   P_i = 1 + d_i / P_{i-1}, Q_i = 1 + d_i / Q_{i-1}
	 *
	 * so, P_1 = A_1 / A_0 = "1 / 0" and Q_1 = B_1 / B_0 = 1
	 *
	 * Further reading on computing the incomplete beta function: [3,4,5,6]
	 */
	
	/* Initialize with P_2, Q_2, f_2 in the following */
	double		d = - (a + b) * x / (a + 1), /* this is d_1 from above */
				P = 1.,    /* which is the same as 1. + d / (1. / EPSILON) */
				Q = 1 + d, /* which is the same as 1. + d / 1. */
				f = 1.0 * P / Q,
				PbyQ;
	
	for (int m = 1; m <= MAX_ITERATIONS; m++) {
		/* We need to be careful with the indices here:
		 * In each iteration, we compute P_{2m+1} = 1 + d_{2m} / P_{2m} and
		 * P_{2m+2} = 1 + d_{2m+1} / P_{2m + 1}. Likewise, for Q.
		 */
		
		d = m * (b - m) * x / ( (a + 2*m - 1) * (a + 2*m) );
		P = 1 + d / P;
		Q = 1 + d / Q;
		
		if (fabs(P) < EPSILON)
			P = (signbit(P) ? -1. : 1.) * EPSILON;
		if (fabs(Q) < EPSILON)
			Q = (signbit(Q) ? -1. : 1.) * EPSILON;

		f = f * P / Q; /* End of computing the (2m+1)-th approximant. */
		
		/* We need to avoid overflows in following iterations. It turns out that
		 * setting P,Q to EPSILON when they are close to zero (or zero) cancels
		 * out in later iterations.
		 *
		 * EPSILON is chosen so to not cause underflow (or overflow). The
		 * literature refers to this technique as "modified Lentz's algorithm"
		 * [2,3,5,6].
		 *
		 * Explanation why setting P,Q to EPSILON works: Suppose P_i ~ 0. Then
		 *   
		 *   P_{i+1} * P_i = (1 + a_{i+1}/P_i) * P_i
		 *                 = P_i + a_{i+1} ~ a_{i+1}.
		 *
		 * Moreover,
		 *             P_n + a_n + P_n * a_{n+2}
		 *   P_{i+2} = -------------------------
		 *                    P_n + a_n
		 *
		 *           ~ 1
		 *
		 * It turns out that setting P_n = EPSILON ensures this in floating-
		 * point arithmetic: In our algorithm we will set:
		 * 
		 *   P_{n+1} = d_{n+1} / EPSILON
		 * 
		 *   f_{n+1} = (f_{n-1} * EPSILON / Q_n) * (d_{n+1} / EPSILON) / Q_{n+1}
		 *
		 * where the EPSILON will cancel out. (Multiplying/dividing by EPSILON 
		 * corresponding to only increasing or decreasing the exponent in the
		 * floating-point representation). Moreover:
		 *
		 *   P_{i+2} = 1 + d_{i+2} / ( d_{n+1} / EPSILON ) = 1
		 *
		 * by the assumptions we have for EPSILON and the d's. */
		
		d = - (a + m) * (a + b + m) * x / ( (a + 2*m) * (a + 2*m + 1) );
		P = 1 + d / P;
		Q = 1 + d / Q;
		
		if (fabs(P) < EPSILON)
			P = (signbit(P) ? -1. : 1.) * EPSILON;
		if (fabs(Q) < EPSILON)
			Q = (signbit(Q) ? -1. : 1.) * EPSILON;
		
		PbyQ = P / Q;
		f = f * PbyQ; /* End of computing the (2m+2)-th approximant. */
		
		if (fabs(PbyQ - 1) < PRECISION)
			break;
	}
	
	return product * f;
}


/*
 * double studentT_cdf(short nu, double t)
 * ---------------------------------------
 *
 * Compute the Pr[T <= t] for Student-t distributed T with nu degrees of
 * freedom.
 */

double studentT_cdf(short nu, double t) {
	/* We use the following identity:
	 *
	 *   Pr[T <= -|t|] = 1/2 * I_{nu/(nu + t^2)} (nu/2, 1/2),
	 * 
	 * where I_x(a,b) denotes the incomplete beta function. */
	
	double		a = .5 * nu,
				b = .5,
				x = nu/(nu + t * t);
	double		I;
	
	/* The term for computing the incomplete beta function includes the factor
	 * x^a (1-x)^b. We use the reflection relation I_x(a,b) = 1 - I_{1-x}(b,a)
	 * to make sure that the larger value of x and (1-x) is raised to nu/2.
	 * Otherwise, we lose convergence speed and risk numerical stability. */
	if (x <= .5)
		I = incompleteBeta_internal(a, b, x);
	else
		I = 1. - incompleteBeta_internal(b, a, 1. - x);
	
	/* The Student-T distribution is obviously symmetric around t=0... */
	if (t < 0)
		return .5 * I;
	else
		return 1. - .5 * I;
}
