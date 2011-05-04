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

#include <madlib/dbal/dbal.hpp>
#include <madlib/modules/modules.hpp>

namespace madlib {

using namespace madlib::dbal;

namespace ports {

// FIXME: we cannot write namespace linux for some reason
namespace linux_ {

#define DECLARE_UDF(NameSpace, Function) DECLARE_UDF_EXT(Function, NameSpace, Function)

#define DECLARE_UDF_EXT(SQLName, NameSpace, Function) \
    extern "C" { \
        __attribute__((visibility("default"))) \
        AnyValue madlib_ ## SQLName(AbstractDBInterface &db, AnyValue args) { \
            return modules::NameSpace::Function(db, args); \
        } \
    }

#include <madlib/modules/declarations.hpp>

#undef DECLARE_UDF_EXT
#undef DECLARE_UDF
#undef MADLIB_INCLUDE_MODULE_HEADERS

} // namespace linux

} // namespace ports

} // namespace madlib
