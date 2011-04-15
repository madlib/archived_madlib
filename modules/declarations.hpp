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

#ifndef MADLIB_DECLARATIONS_HPP
#define MADLIB_DECLARATIONS_HPP


#ifndef NO_PROB
    #include <madlib/modules/prob/student.hpp>
    DECLARE_UDF(prob, student_t_cdf);
#endif

#ifndef NO_REGRESS
    #include <madlib/modules/regress/linear.hpp>
    DECLARE_UDF_EXT(linreg_trans, regress, LinearRegression::transition);
    DECLARE_UDF_EXT(linreg_prelim, regress, LinearRegression::preliminary);
    
    DECLARE_UDF_EXT(linreg_coef_final, regress, LinearRegression::coefFinal);
    DECLARE_UDF_EXT(linreg_r2_final, regress, LinearRegression::RSquareFinal);
    DECLARE_UDF_EXT(linreg_tstats_final, regress, LinearRegression::tStatsFinal);
    DECLARE_UDF_EXT(linreg_pvalues_final, regress, LinearRegression::pValuesFinal);
        
    #include <madlib/modules/regress/logistic.hpp>
    DECLARE_UDF_EXT(logreg_cg_step_trans, regress, LogisticRegressionCG::transition);
    DECLARE_UDF_EXT(logreg_cg_step_prelim, regress, LogisticRegressionCG::preliminary);
    DECLARE_UDF_EXT(logreg_cg_step_final, regress, LogisticRegressionCG::final);
    DECLARE_UDF_EXT(_logreg_cg_step_distance, regress, LogisticRegressionCG::distance);
    DECLARE_UDF_EXT(_logreg_cg_coef, regress, LogisticRegressionCG::coef);

    DECLARE_UDF_EXT(logreg_irls_step_trans, regress, LogisticRegressionIRLS::transition);
    DECLARE_UDF_EXT(logreg_irls_step_prelim, regress, LogisticRegressionIRLS::preliminary);
    DECLARE_UDF_EXT(logreg_irls_step_final, regress, LogisticRegressionIRLS::final);
    DECLARE_UDF_EXT(_logreg_irls_step_distance, regress, LogisticRegressionIRLS::distance);
    DECLARE_UDF_EXT(_logreg_irls_coef, regress, LogisticRegressionIRLS::coef);
#endif

#endif
