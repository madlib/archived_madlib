/* -----------------------------------------------------------------------------
 *
 * @file stats.hpp
 *
 * @brief Umbrella header that includes all statistics headers
 *
 * -------------------------------------------------------------------------- */

namespace madlib {

namespace modules {

/**
 * @brief Statistical tests and correlations coefficients
 */
namespace stats {

#ifdef MADLIB_CURRENT_NAMESPACE
    #undef MADLIB_CURRENT_NAMESPACE
#endif

#define MADLIB_CURRENT_NAMESPACE madlib::modules::stats

#include "chi_squared_test.hpp"
#include "kolmogorov_smirnov_test.hpp"
#include "mann_whitney_test.hpp"
#include "one_way_anova.hpp"
#include "t_test.hpp"
#include "wilcoxon_signed_rank_test.hpp"

} // namespace stats

} // namespace modules

} // namespace madlib
