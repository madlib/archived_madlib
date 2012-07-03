/* ----------------------------------------------------------------------- *//**
 *
 * @file crf.hpp
 *
 *//* ----------------------------------------------------------------------- */
/**
 * @brief Logistic crfion (conjugate-gradient step): Transition function
 */
DECLARE_UDF(crf, linear_crf__step_transition)

/**
 * @brief Logistic crfion (conjugate-gradient step): State merge function
 */
DECLARE_UDF(crf, linear_crf_step_merge_states)

/**
 * @brief Logistic crfion (conjugate-gradient step): Final function
 */
DECLARE_UDF(crf, linear_crf__step_final)

/** 
 * @brief initialization
 */
DECLARE_UDF(crf, init)

/** 
 * @brief compute norm of a vector
 */
DECLARE_UDF(crf, norm)

/** 
 * @brief compute log-likelihood vector gradient 
 */
DECLARE_UDF(crf, compute_logli_gradient)

/** 
 * @brief compute log Mi 
 */
DECLARE_UDF(crf, compute_logli_Mi)

/** 
 * @brief training
 */
DECLARE_UDF(crf, train)
    
/*
    double compute_logli_gradient(double * lambda, double * gradlogli,
		int num_iters, FILE * fout);
		
    void compute_log_Mi(sequence & seq, int pos, doublematrix * Mi,
		doublevector * Vi, int is_exp);
*/

