/* ----------------------------------------------------------------------- *//**
 *
 * @file PGMain.hpp
 *
 * @brief Central header file for PostgreSQL database abstraction layer
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_PGMAIN_HPP
#define MADLIB_POSTGRES_PGMAIN_HPP

#include <dbconnector/PGCommon.hpp>
#include <dbconnector/PGToDatumConverter.hpp>
#include <dbconnector/PGInterface.hpp>
#include <dbconnector/PGValue.hpp>

extern "C" {
    #include <funcapi.h>
    #include <utils/builtins.h>    // needed for format_procedure()
    #include <executor/executor.h> // Greenplum requires this for GetAttributeByNum()
} // extern "C"

namespace madlib {

namespace dbconnector {

/**
 * @brief C++ entry point for calls from the database
 *
 * The DBMS calls an export "C" function defined in PGMain.cpp, which calls
 * this function.
 */
inline static Datum call(MADFunction &f, PG_FUNCTION_ARGS) {
    int sqlerrcode;
    char msg[2048];
    Datum datum;

    try {
        PGInterface db(fcinfo);
        
        try {
            AnyType result = f(db, PGValue<FunctionCallInfo>(fcinfo));

            if (result.isNull())
                PG_RETURN_NULL();
            
            PGToDatumConverter converter(fcinfo);
            return converter.toDatum(result);
        } catch (std::exception &exc) {
            sqlerrcode = ERRCODE_INVALID_PARAMETER_VALUE;
            const char *error = exc.what();
            
            // If there is a pending error, we report this instead.
            if (db.lastError())
                error = db.lastError();
                
            strncpy(msg, error, sizeof(msg));
        }
    } catch (std::exception &exc) {
        sqlerrcode = ERRCODE_INVALID_PARAMETER_VALUE;
        strncpy(msg, exc.what(), sizeof(msg));
    } catch (...) {
        sqlerrcode = ERRCODE_INVALID_PARAMETER_VALUE;
        strncpy(msg,
                "Unknown exception was raised.",
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

} // namespace dbconnector

} // namespace madlib

#endif
