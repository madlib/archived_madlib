/* ----------------------------------------------------------------------- *//**
 *
 * @file linear.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: A header file that does not declare namespaces is to
// be ignored if and only if it is processed stand-alone
#if !defined(_DOXYGEN_IGNORE_HEADER_FILE)

/**
 * @brief Linear regression: Transition function
 */
DECLARE_UDF(linregr_transition)

/**
 * @brief Linear regression: State merge function
 */
DECLARE_UDF(linregr_merge_states)

/**
 * @brief Linear regression: Final function
 */
DECLARE_UDF(linregr_final)

#endif // workaround for Doxygen
