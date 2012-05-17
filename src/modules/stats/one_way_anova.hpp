/* ----------------------------------------------------------------------- *//**
 *
 * @file one_way_anova.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)

/**
 * @brief One-way ANOVA: Transition function
 */
DECLARE_UDF(one_way_anova_transition)

/**
 * @brief One-way ANOVA: State merge function
 */
DECLARE_UDF(one_way_anova_merge_states)

/**
 * @brief One-way ANOVA: Final function
 */
DECLARE_UDF(one_way_anova_final)

#endif // workaround for Doxygen
