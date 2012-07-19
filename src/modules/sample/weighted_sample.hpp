/* ----------------------------------------------------------------------- *//**
 *
 * @file weighted_sample.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Weighted random sample: Transition function
 */
DECLARE_UDF(sample, weighted_sample_transition)

/**
 * @brief Weighted random sample: State merge function
 */
DECLARE_UDF(sample, weighted_sample_merge)

/**
 * @brief Weighted random sample: Final function
 */
DECLARE_UDF(sample, weighted_sample_final)
