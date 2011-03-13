/* ----------------------------------------------------------------------- *//**
 *
 * @file postgres.hpp
 *
 * @brief Central header file for PostgreSQL database abstraction layer
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_HPP
#define MADLIB_POSTGRES_HPP

#include <madlib/madlib.hpp>

#include <madlib/dbal/AnyValue.hpp>
#include <madlib/ports/postgres/PGReturnValue.hpp>
#include <madlib/ports/postgres/PGValue.hpp>

extern "C" {
    #include <postgres.h>
    #include <funcapi.h>
    #include <utils/builtins.h>    // needed for format_procedure()
    #include <executor/executor.h> // Greenplum requires this for GetAttributeByNum()
} // extern "C"

#define DECLARE_UDF_EXT(SQLName, NameSpace, Function) \
    extern "C" { \
        Datum SQLName(PG_FUNCTION_ARGS); \
        PG_FUNCTION_INFO_V1(SQLName); \
        Datum SQLName(PG_FUNCTION_ARGS) { \
            return madlib::ports::postgres::call(madlib::modules::NameSpace::Function(), fcinfo); \
        } \
    }

#define DECLARE_UDF(NameSpace, Function) DECLARE_UDF_EXT(Function, NameSpace, Function)

namespace madlib {

namespace ports {

namespace postgres {

using dbal::AnyValue;

typedef shared_ptr<Datum> DatumSPtr;

template <class Functor>
inline Datum call(const Functor &f, PG_FUNCTION_ARGS) {
    try {
        AnyValue result = f(PGValue<FunctionCallInfo>(fcinfo));

        if (result.isNull())
            PG_RETURN_NULL();
        
        return PGReturnValue(fcinfo, result);
    } catch (std::exception &exc) {
        ereport(ERROR, 
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
            errmsg("Function \"%s\": %s.",
                format_procedure(fcinfo->flinfo->fn_oid),
                exc.what())));
    } catch (...) {
        ereport(ERROR, 
            (errcode(ERRCODE_INVALID_PARAMETER_VALUE),
            errmsg("Function \"%s\": Unknown error. Kindly ask MADlib "
                "developers for a debugging session.",
                format_procedure(fcinfo->flinfo->fn_oid))));
    }
    
    // This will never be reached.
    PG_RETURN_NULL();
}

} // namespace postgres

} // namespace ports

} // namespace madlib

#endif
