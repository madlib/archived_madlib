/* ----------------------------------------------------------------------- *//**
 *
 * @file linear.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if processed as a stand-alone.
#if !defined(_DOXYGEN) || defined(MADLIB_REGRESS_LINEAR_CPP)

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
