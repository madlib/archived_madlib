/* ----------------------------------------------------------------------- *//**
 *
 * @file weighted_sample.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Weighted random sample: Transition function
 */
DECLARE_UDF(sample, weighted_sample_transition_int64)
DECLARE_UDF(sample, weighted_sample_transition_vector)

/**
 * @brief Weighted random sample: State merge function
 */
DECLARE_UDF(sample, weighted_sample_merge_int64)
DECLARE_UDF(sample, weighted_sample_merge_vector)

/**
 * @brief Weighted random sample: Final function
 */
DECLARE_UDF(sample, weighted_sample_final_int64)
DECLARE_UDF(sample, weighted_sample_final_vector)

/**
 * @brief In-memory weighted sample, returning index
 */
DECLARE_UDF(sample, index_weighted_sample)
