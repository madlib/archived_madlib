/* ----------------------------------------------------------------------- *//**
 *
 * @file cox_prop_hazards.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Cox Proportional Hazards: Transition function
 */
DECLARE_UDF(stats, cox_prop_hazards_step_transition)

/**
 * @brief Cox proportional Hazards: Final function
 */
DECLARE_UDF(stats, cox_prop_hazards_step_final)

/**
 * @brief Cox proportional Hazards: Results
 */
DECLARE_UDF(stats, internal_cox_prop_hazards_result)

/**
 * @brief Cox proportional Hazards: Step Distance
 */
DECLARE_UDF(stats, internal_cox_prop_hazards_step_distance)
