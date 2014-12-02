/* ----------------------------------------------------------------------- *//**
 *
 * @file cox_prop_hazards.hpp
 *
 *//* ----------------------------------------------------------------------- */

DECLARE_UDF(stats, coxph_step_inner_final)

/**
 * @brief Cox proportional Hazards: Transition and merge function
 */
DECLARE_UDF(stats, coxph_step_outer_transition)

/**
 * @brief Schoenfeld residuals: Transition and merge function
 */
DECLARE_UDF(stats, zph_transition)
DECLARE_UDF(stats, zph_merge)
DECLARE_UDF(stats, zph_final)

/**
 * @brief Correlation aggregates: Transition, merge, final function
 */
DECLARE_UDF(stats, array_elem_corr_transition)
DECLARE_UDF(stats, array_elem_corr_merge)
DECLARE_UDF(stats, array_elem_corr_final)

/**
 * @brief Metric aggregates: Transition, merge, final function
 */
DECLARE_UDF(stats, coxph_resid_stat_transition)
DECLARE_UDF(stats, coxph_resid_stat_merge)
DECLARE_UDF(stats, coxph_resid_stat_final)

/**
 * @brief Scale residuals
 */
DECLARE_UDF(stats, coxph_scale_resid)
    
/**
 * @Brief Prediction function
 */
DECLARE_UDF(stats, coxph_predict_resp)
DECLARE_UDF(stats, coxph_predict_terms)
