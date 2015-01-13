/* ----------------------------------------------------------------------- *//**
 *
 * @file decision_tree.hpp
 *
 *//* ----------------------------------------------------------------------- */

DECLARE_UDF(recursive_partitioning, initialize_decision_tree)

DECLARE_UDF(recursive_partitioning, compute_leaf_stats_transition)
DECLARE_UDF(recursive_partitioning, compute_leaf_stats_merge)
DECLARE_UDF(recursive_partitioning, dt_apply)

DECLARE_UDF(recursive_partitioning, compute_surr_stats_transition)
DECLARE_UDF(recursive_partitioning, dt_surr_apply)

DECLARE_UDF(recursive_partitioning, print_decision_tree)
DECLARE_UDF(recursive_partitioning, predict_dt_response)
DECLARE_UDF(recursive_partitioning, predict_dt_prob)

DECLARE_UDF(recursive_partitioning, display_decision_tree)
DECLARE_UDF(recursive_partitioning, display_decision_tree_surrogate)
DECLARE_UDF(recursive_partitioning, display_text_tree)

DECLARE_UDF(recursive_partitioning, convert_to_rpart_format)
DECLARE_UDF(recursive_partitioning, get_split_thresholds)
DECLARE_UDF(recursive_partitioning, prune_and_cplist)
    
DECLARE_UDF(recursive_partitioning, convert_to_random_forest_format)
