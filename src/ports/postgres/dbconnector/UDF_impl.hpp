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

// FIXME add some information to explain why we do not need a PG_TRY
// for SRF_FIRSTCALL_INIT(), SRF_RETURN_NEXT() and SRF_RETURN_DONE()
#define MADLIB_SRF_IS_FIRSTCALL() SRF_is_firstcall<Function>(fcinfo)
#define MADLIB_SRF_FIRSTCALL_INIT() SRF_FIRSTCALL_INIT()
#define MADLIB_SRF_PERCALL_SETUP() SRF_percall_setup<Function>(fcinfo)
#define MADLIB_SRF_RETURN_NEXT(_funcctx, _result) SRF_RETURN_NEXT(_funcctx, _result)
#define MADLIB_SRF_RETURN_DONE(_funcctx) SRF_RETURN_DONE(_funcctx)

/**
 * @brief A wrapper for SRF_IS_FIRSTCALL.
 *
 * We wrap SRF_IS_FIRSTCALL inside the PG_TRY block to handle the errors
 *
 * @param fcinfo The PostgreSQL FunctionCallInfoData structure
 */
template <class Function>
inline bool
UDF::SRF_is_firstcall(FunctionCallInfo fcinfo){
    bool is_first_call = false;

    MADLIB_PG_TRY {
        is_first_call = SRF_IS_FIRSTCALL();
    } MADLIB_PG_DEFAULT_CATCH_AND_END_TRY;

    return is_first_call;
}


/**
 * @brief A wrapper for SRF_PERCALL_SETUP.
 *
 * We wrap SRF_PERCALL_SETUP inside the PG_TRY block to handle the errors
 *
 * @param fcinfo The PostgreSQL FunctionCallInfoData structure
 */
template <class Function>
inline FuncCallContext*
UDF::SRF_percall_setup(FunctionCallInfo fcinfo){
    FuncCallContext *fctx = NULL;

    MADLIB_PG_TRY {
        fctx = SRF_PERCALL_SETUP();
    } MADLIB_PG_DEFAULT_CATCH_AND_END_TRY;

    return fctx;
}


/**
 * @brief Internal interface for calling a UDF
 *
 * We need the FunctionCallInfo in case some arguments or return values are
 * of polymorphic types.
 *
 * @param args Arguments to the function. While for calls from the backend
 *     all arguments are specified by \c fcinfo, for calls "within" the C++ AL
 *     it is more efficient to pass arguments as "native" C++ object references.
 */
template <class Function>
inline
AnyType
UDF::invoke(AnyType& args) {
    return Function().run(args);
}


/**
 * @brief Internal interface for calling a set return UDF
 *
 * We need the FunctionCallInfo in case some arguments or return values are
 * of polymorphic types.
 *
 * @param fcinfo The PostgreSQL FunctionCallInfoData structure
 *
 */
template <class Function>
inline
Datum
UDF::SRF_invoke(FunctionCallInfo fcinfo) {
    FuncCallContext *funcctx = NULL;
    MemoryContext oldcontext;
    bool is_last_call = false;

    if (MADLIB_SRF_IS_FIRSTCALL()) {
        /* create a function context for cross-call persistence */
        funcctx = MADLIB_SRF_FIRSTCALL_INIT();
        oldcontext = MemoryContextSwitchTo(funcctx->multi_call_memory_ctx);

        // we must construct the argument here, since it needs SRF_FIRSTCALL_INIT
        // to init the pointer: fn_extra
        AnyType args(fcinfo);
        args.mSysInfo->user_fctx = Function().SRF_init(args);
        MemoryContextSwitchTo(oldcontext);
    }

    funcctx = MADLIB_SRF_PERCALL_SETUP();

    // the invoker function will handle the exceptions from this function
    // TODO: Don't we need args for "next"?
    AnyType result = Function().SRF_next(
        static_cast<SystemInformation*>(funcctx->user_fctx)->user_fctx,
        &is_last_call);

    if (is_last_call)
        MADLIB_SRF_RETURN_DONE(funcctx);

    Datum datum;
    if (result.isNull()) {
        fcinfo->isnull = true;
        datum = (Datum) 0;
    } else {
        datum = result.getAsDatum(fcinfo);
    }

    MADLIB_SRF_RETURN_NEXT(funcctx, datum);
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

        // FIXME: Rethink/redesign support for set-returning functions
        // See also UDF_proto.hpp
        if (fcinfo->flinfo->fn_retset) {
            return SRF_invoke<Function>(fcinfo);
        } else {
            SystemInformation::get(fcinfo)
                ->functionInformation(fcinfo->flinfo->fn_oid)->cxx_func
                = invoke<Function>;

            AnyType args(fcinfo);
            AnyType result = invoke<Function>(args);

            if (result.isNull())
                PG_RETURN_NULL();

            return result.getAsDatum(fcinfo);
        }
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
