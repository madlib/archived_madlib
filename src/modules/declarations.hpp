/* ----------------------------------------------------------------------- *//**
 *
 * @file declarations.hpp
 *
 * @brief Declaration of all functions, used by all platforms with "reflection"
 *
 *//* -------------------------------------------------------------------- *//**
 *
 * @file declarations.hpp
 *
 * FIXME: Description outdated
 * These declarations are used by all platform ports that support "reflection",
 * i.e., where all functions share the same C/C++ interface and the platform 
 * provides functionality to getting the argument list (and the respective
 * argument and return types). 
 *
 * Each compliant platform port must provide the following two macros:
 * @code
 * DECLARE_UDF_EXT(SQLName, NameSpace, Function)
 * DECLARE_UDF(NameSpace, Function)
 * @endcode
 * where \c SQLName is the external name (which the database will use as entry
 * point when calling the madlib library) and \c Function is the internal class
 * name implementing the UDF.
 */

// prob/chiSquared.hpp
DECLARE_UDF(prob, chi_squared_cdf)
DECLARE_UDF(prob, chi_squared_pdf)
DECLARE_UDF(prob, chi_squared_quantile)

// prob/student.hpp
DECLARE_UDF(prob, students_t_cdf)
DECLARE_UDF(prob, students_t_pdf)
DECLARE_UDF(prob, students_t_quantile)

// prob/gamma.hpp
DECLARE_UDF(prob, gamma_cdf)
DECLARE_UDF(prob, gamma_pdf)
DECLARE_UDF(prob, gamma_quantile)

// prob/beta.hpp
DECLARE_UDF(prob, beta_cdf)
DECLARE_UDF(prob, beta_pdf)
DECLARE_UDF(prob, beta_quantile)

// regress/linear.hpp
DECLARE_UDF_EXT(linregr_transition, regress, LinearRegression::transition)
DECLARE_UDF_EXT(linregr_merge_states, regress, LinearRegression::mergeStates)
DECLARE_UDF_EXT(linregr_final, regress, LinearRegression::final)
    
// regress/logistic.hpp
DECLARE_UDF_EXT(logregr_cg_step_transition, regress, LogisticRegressionCG::transition)
DECLARE_UDF_EXT(logregr_cg_step_merge_states, regress, LogisticRegressionCG::mergeStates)
DECLARE_UDF_EXT(logregr_cg_step_final, regress, LogisticRegressionCG::final)
DECLARE_UDF_EXT(internal_logregr_cg_step_distance, regress, LogisticRegressionCG::distance)
DECLARE_UDF_EXT(internal_logregr_cg_result, regress, LogisticRegressionCG::result)

DECLARE_UDF_EXT(logregr_irls_step_transition, regress, LogisticRegressionIRLS::transition)
DECLARE_UDF_EXT(logregr_irls_step_merge_states, regress, LogisticRegressionIRLS::mergeStates)
DECLARE_UDF_EXT(logregr_irls_step_final, regress, LogisticRegressionIRLS::final)
DECLARE_UDF_EXT(internal_logregr_irls_step_distance, regress, LogisticRegressionIRLS::distance)
DECLARE_UDF_EXT(internal_logregr_irls_result, regress, LogisticRegressionIRLS::result)
