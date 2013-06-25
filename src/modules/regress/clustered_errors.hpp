
/* ----------------------------------------------------------------------- *//**
 *
 * @file clustered_errors.hpp
 *
 *//* ----------------------------------------------------------------------- */

// transition, merge and final functions for linear regression clustered errors

DECLARE_UDF(regress, __clustered_err_lin_transition)

DECLARE_UDF(regress, __clustered_err_lin_merge)

DECLARE_UDF(regress, __clustered_err_lin_final)

DECLARE_UDF(regress, clustered_compute_lin_stats)
