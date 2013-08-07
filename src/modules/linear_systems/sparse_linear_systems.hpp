/* ----------------------------------------------------------------------- *//**
 *
 * @file sparse_linear_systems.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Direct sparse Linear Systems: Transition function
 */
DECLARE_UDF(linear_systems, sparse_direct_linear_system_transition)

/**
 * @brief Direct sparse Linear Systems: State Merge function
 */
DECLARE_UDF(linear_systems, sparse_direct_linear_system_merge_states)

/**
 * @brief Direct sparse Linear Systems: Final function
 */
DECLARE_UDF(linear_systems, sparse_direct_linear_system_final)



/**
 * @brief inmem_iterative sparse Linear Systems: Transition function
 */
DECLARE_UDF(linear_systems, sparse_inmem_iterative_linear_system_transition)

/**
 * @brief inmem_iterative sparse Linear Systems: State Merge function
 */
DECLARE_UDF(linear_systems, sparse_inmem_iterative_linear_system_merge_states)

/**
 * @brief inmem_iterative sparse Linear Systems: Final function
 */
DECLARE_UDF(linear_systems, sparse_inmem_iterative_linear_system_final)

