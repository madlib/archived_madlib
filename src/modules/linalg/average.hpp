/* ----------------------------------------------------------------------- *//**
 *
 * @file average.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Average of vectors: Transition function
 */
DECLARE_UDF(linalg, avg_vector_transition)

/**
 * @brief Average of vectors: State merge function
 */
DECLARE_UDF(linalg, avg_vector_merge)

/**
 * @brief Average of vectors: Final function
 */
DECLARE_UDF(linalg, avg_vector_final)


/**
 * @brief Normalized average of vectors: Transition function
 */
DECLARE_UDF(linalg, normalized_avg_vector_transition)

/**
 * @brief Normalized average of vectors: Final function
 */
DECLARE_UDF(linalg, normalized_avg_vector_final)
