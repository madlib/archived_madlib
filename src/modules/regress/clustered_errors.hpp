
/* ----------------------------------------------------------------------- *//**
 *
 * @file clustered_errors.hpp
 *
 *//* ----------------------------------------------------------------------- */

// transition, merge and final functions for linear regression clustered errors

DECLARE_UDF(regress, __clustered_err_lin_transition)

DECLARE_UDF(regress, __clustered_err_lin_merge)

DECLARE_UDF(regress, __clustered_err_lin_final)

DECLARE_UDF(regress, clustered_lin_compute_stats)

DECLARE_UDF(regress, __clustered_err_log_transition)

DECLARE_UDF(regress, __clustered_err_log_merge)

DECLARE_UDF(regress, __clustered_err_log_final)

DECLARE_UDF(regress, clustered_log_compute_stats)


DECLARE_UDF(regress, __clustered_err_mlog_transition)

DECLARE_UDF(regress, __clustered_err_mlog_merge)

DECLARE_UDF(regress, __clustered_err_mlog_final)

DECLARE_UDF(regress, clustered_mlog_compute_stats)

