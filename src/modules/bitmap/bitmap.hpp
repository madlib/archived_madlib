/* ----------------------------------------------------------------------- *//**
 *
 * @file bitmap.hpp
 *
 * The functions' declarations for the plc functions in bitmap.sql_in
 *
 *//* ----------------------------------------------------------------------- */


DECLARE_UDF(bitmap, bitmap_agg_sfunc)
DECLARE_UDF(bitmap, bitmap_agg_pfunc)
DECLARE_UDF(bitmap, bitmap_and)
DECLARE_UDF(bitmap, bitmap_or)
DECLARE_UDF(bitmap, bitmap_xor)
DECLARE_UDF(bitmap, bitmap_not)
DECLARE_UDF(bitmap, bitmap_set)
DECLARE_UDF(bitmap, bitmap_test)
DECLARE_UDF(bitmap, bitmap_out)
DECLARE_UDF(bitmap, bitmap_in)
DECLARE_UDF(bitmap, bitmap_return_array)
DECLARE_UDF(bitmap, bitmap_return_varbit)
DECLARE_UDF(bitmap, bitmap_nonzero_count)
DECLARE_UDF(bitmap, bitmap_nonzero_positions)
DECLARE_UDF(bitmap, bitmap_from_int8array)
DECLARE_UDF(bitmap, bitmap_from_int4array)
DECLARE_UDF(bitmap, bitmap_from_varbit)
DECLARE_UDF(bitmap, bitmap_eq)
DECLARE_UDF(bitmap, bitmap_neq)
DECLARE_UDF(bitmap, bitmap_le)
DECLARE_UDF(bitmap, bitmap_ge)
DECLARE_UDF(bitmap, bitmap_lt)
DECLARE_UDF(bitmap, bitmap_gt)
DECLARE_UDF(bitmap, bitmap_cmp)
DECLARE_SR_UDF(bitmap, bitmap_unnest)


