/* ----------------------------------------------------------------------- *//**
 *
 * @file chi_squared_test.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Pearson's chi-squared test: Transition function
 */
DECLARE_UDF(stats, chi2_gof_test_transition)

/**
 * @brief Pearson's chi-squared test: State merge function
 */
DECLARE_UDF(stats, chi2_gof_test_merge_states)

/**
 * @brief Pearson's chi-squared test: Final function
 */
DECLARE_UDF(stats, chi2_gof_test_final)
