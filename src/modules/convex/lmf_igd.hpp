/* ----------------------------------------------------------------------- *//**
 *
 * @file lmf_igd.hpp
 *
 *//* ----------------------------------------------------------------------- */

/**
 * @brief Low-rank matrix factorization (incremental gradient): Transition function
 */
DECLARE_UDF(convex, lmf_igd_transition)

/**
 * @brief Low-rank matrix factorization (incremental gradient): State merge function
 */
DECLARE_UDF(convex, lmf_igd_merge)

/**
 * @brief Low-rank matrix factorization (incremental gradient): Final function
 */
DECLARE_UDF(convex, lmf_igd_final)

/**
 * @brief Low-rank matrix factorization (incremental gradient): Difference in
 *     log-likelihood between two transition states
 */
DECLARE_UDF(convex, internal_lmf_igd_distance)

/**
 * @brief Low-rank matrix factorization (incremental gradient): Convert
 *     transition state to result tuple
 */
DECLARE_UDF(convex, internal_lmf_igd_result)

