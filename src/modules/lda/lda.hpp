/* ----------------------------------------------------------------------- *//**
 *
 * @file lda.hpp
 *
 * @brief Parallel Latent Dirichlet Allocation
 *
 *//* ----------------------------------------------------------------------- */

DECLARE_UDF(lda, lda_random_assign)
DECLARE_UDF(lda, lda_gibbs_sample)

DECLARE_UDF(lda, lda_count_topic_sfunc)
DECLARE_UDF(lda, lda_count_topic_prefunc)

DECLARE_UDF(lda, lda_transpose)
DECLARE_SR_UDF(lda, lda_unnest_transpose)
DECLARE_SR_UDF(lda, lda_unnest)

DECLARE_UDF(lda, lda_perplexity_sfunc)
DECLARE_UDF(lda, lda_perplexity_prefunc)
DECLARE_UDF(lda, lda_perplexity_ffunc)

DECLARE_UDF(lda, lda_check_count_ceiling)

DECLARE_UDF(lda, l1_norm_with_smoothing)
DECLARE_UDF(lda, lda_parse_model)
