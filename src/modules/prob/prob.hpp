/* -----------------------------------------------------------------------------
 *
 * @file prob.hpp
 *
 * @brief Umbrella header that includes all probability functions
 *
 * -------------------------------------------------------------------------- */

namespace madlib {

namespace modules {

/**
 * @brief Probability functions
 */
namespace prob {

#ifdef MADLIB_CURRENT_NAMESPACE
    #undef MADLIB_CURRENT_NAMESPACE
#endif

#define MADLIB_CURRENT_NAMESPACE madlib::modules::prob
#include "bernoulli.hpp"
#include "beta.hpp"
#include "binomial.hpp"
#include "cauchy.hpp"
#include "chi_squared.hpp"
#include "exponential.hpp"
#include "fisher_f.hpp"
#include "gamma.hpp"
#include "kolmogorov.hpp"
#include "laplace.hpp"
#include "non_central_t.hpp"
#include "normal.hpp"
#include "pareto.hpp"
#include "rayleigh.hpp"
#include "students_t.hpp"
#include "triangular.hpp"
#include "uniform.hpp"
#include "weibull.hpp"
} // namespace prob

} // namespace modules

} // namespace madlib
