/* ----------------------------------------------------------------------- *//**
 *
 * @file UDF_impl.hpp
 *
 *//* ----------------------------------------------------------------------- */

#ifndef MADLIB_POSTGRES_UDF_IMPL_HPP
#define MADLIB_POSTGRES_UDF_IMPL_HPP

namespace madlib {

namespace dbconnector {

namespace postgres {

#define MADLIB_HANDLE_STANDARD_EXCEPTION(err) \
    sqlerrcode = err; \
    strncpy(msg, exc.what(), sizeof(msg));

/**
 * @brief Internal interface for calling a UDF
 *
 * We need the FunctionCallInfo in case some arguments or return values are
 * of polymorphic types.
 *
 * @param fcinfo The PostgreSQL FunctionCallInfoData structure
 * @param args Arguments to the function. While for calls from the backend
 *     all arguments are specified by \c fcinfo, for calls "within" the C++ AL
 *     it is more efficient to pass arguments as "native" C++ object references.
 *     This is 
 */
template <class Function>
inline
AnyType
UDF::invoke(FunctionCallInfo fcinfo, AnyType& args) {
    return Function(fcinfo).run(args);
}

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
        // We want to store in the cache that this function is implemented on
        // top of the C++ AL. Should the same function be invoked again via a
        // FunctionHandle, it can be invoked directly.
        SystemInformation::get(fcinfo)
            ->functionInformation(fcinfo->flinfo->fn_oid)->cxx_func
            = invoke<Function>;

        AnyType args(fcinfo);
        AnyType result = invoke<Function>(fcinfo, args);

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

} // namespace postgres

} // namespace dbconnector

} // namespace madlib

#endif // defined(MADLIB_POSTGRES_UDF_IMPL_HPP)
