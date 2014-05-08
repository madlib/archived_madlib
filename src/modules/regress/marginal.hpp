/**
 * @brief Marginal Effects Logistic regression step: Transition function
 */
DECLARE_UDF(regress, margins_logregr_int_transition)

/**
 * @brief Marginal effects Logistic regression: State merge function
 */
DECLARE_UDF(regress, margins_logregr_int_merge)

/**
 * @brief Marginal Effects Logistic regression: Final function
 */
DECLARE_UDF(regress, margins_logregr_int_final)

/**
 * @brief Marginal Effects Linear regression step: Transition function
 */
DECLARE_UDF(regress, margins_linregr_int_transition)

/**
 * @brief Marginal effects linear regression: State merge function
 */
DECLARE_UDF(regress, margins_linregr_int_merge)

/**
 * @brief Marginal Effects linear regression: Final function
 */
DECLARE_UDF(regress, margins_linregr_int_final)

/**
 * @brief Marginal Effects multinomial logistic regression step: Transition function
 */
DECLARE_UDF(regress, margins_mlogregr_int_transition)

/**
 * @brief Marginal effects multinomial logistic regression: State merge function
 */
DECLARE_UDF(regress, margins_mlogregr_int_merge)

/**
 * @brief Marginal Effects multinomial logistic regression: Final function
 */
DECLARE_UDF(regress, margins_mlogregr_int_final)
