/* ----------------------------------------------------------------------- *//**
 *
 * @file kmeans.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief k-Means: Transition function
 */
DECLARE_UDF(kmeans, kmeans_step_transition)

/**
 * @brief k-Means: State merge function
 */
DECLARE_UDF(kmeans, kmeans_step_merge)

/**
 * @brief k-Means: Final function
 */
DECLARE_UDF(kmeans, kmeans_step_final)
