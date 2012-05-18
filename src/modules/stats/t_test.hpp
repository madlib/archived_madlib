/* ----------------------------------------------------------------------- *//**
 *
 * @file t_test.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief One-sample t-Test: Transition function
 */
DECLARE_UDF(stats, t_test_one_transition)

/**
 * @brief Two-sample t-Test: Transition function
 */
DECLARE_UDF(stats, t_test_two_transition)

/**
 * @brief t-Test: State merge function
 */
DECLARE_UDF(stats, t_test_merge_states)

/**
 * @brief One-sample t-Test: Final function
 */
DECLARE_UDF(stats, t_test_one_final)

/**
 * @brief Two-sample pooled t-Test: Final function
 */
DECLARE_UDF(stats, t_test_two_pooled_final)

/**
 * @brief Two-sample unpooled t-Test: Final function
 */
DECLARE_UDF(stats, t_test_two_unpooled_final)


/**
 * @brief Two-sample unpooled t-Test: Final function
 */
DECLARE_UDF(stats, f_test_final)
