/* ----------------------------------------------------------------------- *//**
 *
 * @file cox_prop_hazards.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Cox Proportional Hazards: Transition function
 */
DECLARE_UDF(stats, coxph_step_transition)

/**
 * @brief Cox proportional Hazards: Final function
 */
DECLARE_UDF(stats, coxph_step_final)

DECLARE_UDF(stats, coxph_step_strata_final)

DECLARE_UDF(stats, coxph_step_inner_final)

/**
 * @brief Cox proportional Hazards: Results
 */
DECLARE_UDF(stats, internal_coxph_result)

/**
 * @brief Cox proportional Hazards: Step Distance
 */
DECLARE_UDF(stats, internal_coxph_step_distance)

/**
 * @brief Cox proportional Hazards: Transition and merge function
 */
DECLARE_UDF(stats, coxph_step_outer_transition)
