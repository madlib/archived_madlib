/* ----------------------------------------------------------------------- *//**
 *
 * @file dense_linear_systems.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Direct Dense Linear Systems: Transition function
 */
DECLARE_UDF(linear_systems, dense_direct_linear_system_transition)

/**
 * @brief Direct Dense Linear Systems: State Merge function
 */
DECLARE_UDF(linear_systems, dense_direct_linear_system_merge_states)

/**
 * @brief Direct Dense Linear Systems: Final function
 */
DECLARE_UDF(linear_systems, dense_direct_linear_system_final)



/**
 * @brief Direct Dense Linear Systems: Transition function
 */
DECLARE_UDF(linear_systems, dense_residual_norm_transition)

/**
 * @brief Direct Dense Linear Systems: State Merge function
 */
DECLARE_UDF(linear_systems, dense_residual_norm_merge_states)

/**
 * @brief Direct Dense Linear Systems: Final function
 */
DECLARE_UDF(linear_systems, dense_residual_norm_final)
