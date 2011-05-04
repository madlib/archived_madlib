/* ----------------------------------------------------------------------- *//**
 *
 * @file declarations.cpp
 *
 * On Linux, we only export C names: E.g., if the SQL name is logreg_coef and
 * the function called is LogisticRegression::coef, then we define an extern "C"
 * function madlib_logreg_coef that calls LogisticRegression::coef. Only
 * madlib_logreg_coef will be exported. The reason is that on Linux we will be
 * loaded with dlopen, and the function lookup happens with dlsym (where mangled
 * C++ would be a major inconvenience).
 * 
 * The short reason for why dlopen is used on Linux is that Linux only has a
 * flat namespace for shared libraries. The long reason is explained in
 * libDependencyFix.c.
 *
 *//* ----------------------------------------------------------------------- */

#define MADLIB_INCLUDE_MODULES_HEADERS

#define DECLARE_UDF(NameSpace, Function) DECLARE_UDF_EXT(Function, NameSpace, Function)

#define DECLARE_UDF_EXT(SQLName, NameSpace, Function) \
    extern "C" { \
        __attribute__((visibility("default"))) \
        Datum madlib_ ## SQLName(AbstractDBInterface &db, AnyValue args) { \
            return madlib::modules::NameSpace::Function(db, args); \
        } \
    }

#include <madlib/modules/declarations.hpp>

#undef MADLIB_INCLUDE_MODULES_HEADERS
#undef DECLARE_UDF_EXT
#undef DECLARE_UDF
