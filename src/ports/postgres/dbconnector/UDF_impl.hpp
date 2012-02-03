/* ----------------------------------------------------------------------- *//**
 *
 * @file UDF_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

// Workaround for Doxygen: Ignore if not included by dbconnector.hpp
#ifdef MADLIB_DBCONNECTOR_HPP

#define MADLIB_HANDLE_STANDARD_EXCEPTION(err) \
    sqlerrcode = err; \
    strncpy(msg, exc.what(), sizeof(msg));

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
    } catch (std::invalid_argument& exc) {
        MADLIB_HANDLE_STANDARD_EXCEPTION(ERRCODE_INVALID_PARAMETER_VALUE);
    } catch (std::domain_error& exc) {
        MADLIB_HANDLE_STANDARD_EXCEPTION(ERRCODE_INVALID_PARAMETER_VALUE);
    } catch (std::range_error& exc) {
        MADLIB_HANDLE_STANDARD_EXCEPTION(ERRCODE_DATA_EXCEPTION);
    } catch (std::overflow_error& exc) {
        MADLIB_HANDLE_STANDARD_EXCEPTION(ERRCODE_DATA_EXCEPTION);
    } catch (std::underflow_error& exc) {
        MADLIB_HANDLE_STANDARD_EXCEPTION(ERRCODE_DATA_EXCEPTION);
    } catch (dbal::NoSolutionFoundException& exc) {
        MADLIB_HANDLE_STANDARD_EXCEPTION(ERRCODE_DATA_EXCEPTION);
    } catch (std::exception& exc) {
        MADLIB_HANDLE_STANDARD_EXCEPTION(ERRCODE_INTERNAL_ERROR);
    } catch (...) {
        sqlerrcode = ERRCODE_INTERNAL_ERROR;
        strncpy(msg,
            "Internal error: Unknown exception was raised.",
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

#undef MADLIB_HANDLE_STANDARD_EXCEPTION

#endif // MADLIB_DBCONNECTOR_HPP (workaround for Doxygen)
