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
    DECLARE_UDF_EXT(linreg_final, regress, LinearRegression::coefFinal);
#endif


#endif
