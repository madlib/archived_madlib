/* ----------------------------------------------------------------------- *//**
 *
 * @file feature_encoding.hpp
 *
 *//* ----------------------------------------------------------------------- */

DECLARE_UDF(recursive_partitioning, dst_compute_con_splits_transition)
DECLARE_UDF(recursive_partitioning, dst_compute_con_splits_final)

DECLARE_UDF(recursive_partitioning, dst_compute_entropy_transition)
DECLARE_UDF(recursive_partitioning, dst_compute_entropy_merge)
DECLARE_UDF(recursive_partitioning, dst_compute_entropy_final)

DECLARE_UDF(recursive_partitioning, map_catlevel_to_int)
DECLARE_UDF(recursive_partitioning, print_con_splits)

DECLARE_UDF(recursive_partitioning, get_bin_value_by_index)
DECLARE_UDF(recursive_partitioning, get_bin_index_by_value)
DECLARE_UDF(recursive_partitioning, get_bin_indices_by_values)
