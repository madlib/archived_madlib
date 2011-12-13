/**
 * @brief Each exported C function calls this method (and nothing else)
 */
template <class Function>
inline
Datum
UDF::call(FunctionCallInfo fcinfo) {
    int sqlerrcode;
    char msg[2048];

    try {
        try {
            AnyType args = fcinfo;
            AnyType result = Function(fcinfo).run(args);

            if (result.isNull())
                PG_RETURN_NULL();
            
            return result.getAsDatum(fcinfo);
        } catch (std::bad_alloc &) {
            sqlerrcode = ERRCODE_OUT_OF_MEMORY;
            strncpy(msg,
                "Memory allocation failed. Typically, this indicates that "
                PACKAGE_NAME
                " limits the available memory to less than what is needed for this "
                "input.",
                sizeof(msg));
        } catch (std::exception &exc) {
            sqlerrcode = ERRCODE_INVALID_PARAMETER_VALUE;
            const char *error = exc.what();
                        
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

