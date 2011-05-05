/* ----------------------------------------------------------------------- *//**
 *
 * @file main.hpp
 *
 * @brief Central header file for PostgreSQL database abstraction layer
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_MAIN_HPP
#define MADLIB_POSTGRES_MAIN_HPP

#include <madlib/ports/postgres/postgres.hpp>
#include <madlib/ports/postgres/PGToDatumConverter.hpp>
#include <madlib/ports/postgres/PGInterface.hpp>
#include <madlib/ports/postgres/PGValue.hpp>

extern "C" {
    #include <funcapi.h>
    #include <utils/builtins.h>    // needed for format_procedure()
    #include <executor/executor.h> // Greenplum requires this for GetAttributeByNum()
} // extern "C"

namespace madlib {

namespace ports {

namespace postgres {

inline static Datum call(
    MADFunction &f,
    PG_FUNCTION_ARGS) {
//template <AnyValue f(AbstractDBInterface &, AnyValue)>
//inline Datum call(PG_FUNCTION_ARGS) {
    int sqlerrcode;
    char msg[256];
    Datum datum;

    try {
        PGInterface db(fcinfo);
        AnyValue result = f(db, PGValue<FunctionCallInfo>(fcinfo));

        if (result.isNull())
            PG_RETURN_NULL();
        
        datum = PGToDatumConverter(fcinfo, result);
        return datum;
    } catch (std::exception &exc) {
        sqlerrcode = ERRCODE_INVALID_PARAMETER_VALUE;
        strncpy(msg, exc.what(), sizeof(msg));
    } catch (...) {
        sqlerrcode = ERRCODE_INVALID_PARAMETER_VALUE;
        strncpy(msg,
            "Unknown error. Kindly ask MADlib developers for a "
            "debugging session.",
            sizeof(msg));
    }
    
    // This code will only be reached in case of error.
    // We want to ereport only here, with only POD (plain old data) left on the
    // stack. (ereport will do a longjmp)
    msg[sizeof(msg) - 1] = '\0';
    ereport (
        ERROR, (
            errcode(sqlerrcode),
            errmsg(
                "Function \"%s\": %s",
                format_procedure(fcinfo->flinfo->fn_oid),
                msg
            )
        )
    );
    
    // This will never be reached.
    PG_RETURN_NULL();
}

} // namespace postgres

} // namespace ports

} // namespace madlib

#endif
