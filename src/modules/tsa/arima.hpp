/* ----------------------------------------------------------------------- *//**
 *
 * @file arima.hpp
 *
 * @brief ARIMA 
 *
 *//* ----------------------------------------------------------------------- */

DECLARE_UDF(tsa, arima_residual)

DECLARE_UDF(tsa, arima_diff)
DECLARE_UDF(tsa, arima_adjust)

DECLARE_UDF(tsa, arima_lm_delta)

DECLARE_UDF(tsa, arima_lm)

DECLARE_UDF(tsa, arima_lm_result_sfunc)
DECLARE_UDF(tsa, arima_lm_result_pfunc)
DECLARE_UDF(tsa, arima_lm_result_ffunc)

DECLARE_UDF(tsa, arima_lm_stat_sfunc)
DECLARE_UDF(tsa, arima_lm_stat_ffunc)
