/* ----------------------------------------------------------------------- *//**
 *
 * @file PGMain.cpp
 *
 * @brief PostgreSQL database abstraction layer 
 *
 *//* -------------------------------------------------------------------- *//**
 *
 * Postgres is a platform where the C interface supports reflection, so all we
 * need to do is to include the PostgreSQL database abstraction layer and the
 * default declarations.
 */

#include <dbconnector/PGMain.hpp>
#include <modules/modules.hpp>

namespace madlib {

namespace dbconnector {

extern "C" {
    PG_MODULE_MAGIC;
} // extern "C"

#define DECLARE_UDF(NameSpace, Function) DECLARE_UDF_EXT(Function, NameSpace, Function)

#define DECLARE_UDF_EXT(SQLName, NameSpace, Function) \
    extern "C" { \
        Datum SQLName(PG_FUNCTION_ARGS); \
        PG_FUNCTION_INFO_V1(SQLName); \
        Datum SQLName(PG_FUNCTION_ARGS) { \
            return call( \
                modules::NameSpace::Function, \
                fcinfo); \
        } \
    }

#include <modules/declarations.hpp>

#undef DECLARE_UDF_EXT
#undef DECLARE_UDF

} // namespace dbconnector

} // namespace madlib
