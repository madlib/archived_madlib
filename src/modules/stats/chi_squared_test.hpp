/* ----------------------------------------------------------------------- *//**
 *
 * @file chi_squared_test.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)

/**
 * @brief Pearson's chi-squared test: Transition function
 */
DECLARE_UDF(chi2_gof_test_transition)

/**
 * @brief Pearson's chi-squared test: State merge function
 */
DECLARE_UDF(chi2_gof_test_merge_states)

/**
 * @brief Pearson's chi-squared test: Final function
 */
DECLARE_UDF(chi2_gof_test_final)

#endif // workaround for Doxygen
