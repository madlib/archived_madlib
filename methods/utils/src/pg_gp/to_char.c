#include <postgres.h>
#include <fmgr.h>
#include <utils/lsyscache.h>
#include <utils/builtins.h>

#ifndef NO_PG_MODULE_MAGIC
PG_MODULE_MAGIC;
#endif

PG_FUNCTION_INFO_V1(__to_text);
/*
 * @brief Cast a value to text. On some databases, there
 *        are no such casts for certain data types, such as
 *        the cast for bit to text. 
 *
 * @param value   The value with any specific type
 *
 * @note This is a strict function.
 *
 */
Datum
__to_text(PG_FUNCTION_ARGS) {
    Datum   value            = PG_GETARG_DATUM(0);
    Oid     valtype          = get_fn_expr_argtype(fcinfo->flinfo, 0);
    Oid		typoutput        = 0;
    bool    typIsVarlena     = 0;
    char   *result           = NULL;

    getTypeOutputInfo(valtype, &typoutput, &typIsVarlena);

    // call the output function of the type to convert 
    result = OidOutputFunctionCall(typoutput, value);

    PG_RETURN_TEXT_P(cstring_to_text(result));
}

