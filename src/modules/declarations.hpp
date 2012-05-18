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
 * DECLARE_UDF(ExportedName)
 * DECLARE_UDF_WITH_POLICY(ExportedName)
 * @endcode
 * where \c ExportedName is the external name (which the database will use as
 * entry point when calling the madlib library).
 */

#include <modules/prob/prob.hpp>
#include <modules/regress/regress.hpp>
#include <modules/stats/stats.hpp>
