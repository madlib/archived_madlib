/* ----------------------------------------------------------------------- */
/**
 *
 * @file kolmogorov.cpp
 *
 * @brief Kolmogorov Probability distribution function
 *
 *
 */
/* ----------------------------------------------------------------------- */

#include <dbconnector/dbconnector.hpp>

#include "kolmogorov.hpp"

namespace madlib {
namespace modules {
namespace prob {

/**
 * @brief Komogorov cumulative distribution function: In-database interface
 */
AnyType
kolmogorov_cdf::run(AnyType &args) {
    return prob::cdf(kolmogorov(), args[0].getAs<double>());
}

double KolmogorovProb(double z);

/**
 * @brief  Calculates the Kolmogorov distribution function
 * @param  x Value to compare with
 * @return   Output probability that Kolmogorov's test statistic will exceed
 *            the value x assuming the null hypothesis.
 *
 *    The implementation is based on the method described in
 *    http://www.jstatsoft.org/v08/i18/paper - Section 3
 *    The limiting forms in the paper correspond to P(K <= x); since we need
 *    the P(K > x) we return 1 - P(K <= x).
 */
double KolmogorovProb(double x) {
   double u = std::fabs(x);
   double p = 0;
   if (u < 0.1) {
        // probability too close to 1
        p = 1;
    } else if (u <= 1) {
        // for small u we use the theta function inversion formula
        // p = 1 - (sqrt(2*pi)/u) * sum e^( -((2i-1) pi)^2 / (8u^2) )
        const double v = 1./(8*u*u);
        const double k1 = -v * pow(M_PI, 2);
        const double k2 = 9 * k1;
        const double k3 = 25 * k1;
        const double k4 = 49 * k1;
        const double w = sqrt(2 * M_PI) / u;
        p = 1 - w * (std::exp(k1) + std::exp(k2) + std::exp(k3) + std::exp(k4));
    } else if (u < 5) {
        // p = 2 * sum -1^(i-1) * e^( -2 * i^2 * u^2) )
        double prob_sum = 0;
        double sign = 1;
        double v = u*u;
        uint16_t maxi = std::max(uint16_t(1), uint16_t(4./u + 0.5));
        // since u is > 1, 4/u will be < 4
        for (uint16_t i=1; i <= maxi && i <= 4; i++) {
            prob_sum += sign * std::exp(-2 * (i*i) * v);
            sign = -sign;  //
        }
        p = 2 * prob_sum;
    } else {
        p = 0;
    }
   return p;
}

} // namespace prob
} // namespace modules
} // namespace regress
