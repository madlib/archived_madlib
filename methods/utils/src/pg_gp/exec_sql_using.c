#include <postgres.h>
#include <fmgr.h>
#include <funcapi.h>
#include <catalog/pg_proc.h>
#include <catalog/pg_type.h>
#include <utils/builtins.h>
#if PG_VERSION_NUM >= 100000
#include <utils/regproc.h>
#endif
#include <utils/datum.h>
#include <utils/lsyscache.h>
#include <utils/syscache.h>

// On Greenplum 4.2.0, spi.h indirectly includes <emcconnect/api.h>. However,
// this file is not shipped with GPDB 4.2 (MPP-15620). We undefine
// USE_CONNECTEMC here as a workaround.
#if defined(GP_VERSION_NUM) && GP_VERSION_NUM == 40200
    #if defined(USE_CONNECTEMC)
        #undef USE_CONNECTEMC
    #endif  // defined(USE_CONNECTEMC)
#endif // defined(GP_VERSION_NUM) && GP_VERSION_NUM == 40200

#include <executor/spi.h>

// SearchSysCache1 first appeared with commit e26c539 by Robert Haas
// <rhaas@postgresql.org> on Sun, 14 Feb 2010 18:42:19 +0000. If this macro is
// not defined, we assume to be on a version of PostgreSQL that expects five
// arguments to SearchSysCache().
#if !defined(SearchSysCache1)
    #define SearchSysCache1(cacheId, key1) \
        SearchSysCache(cacheId, key1, 0, 0, 0)
#endif // defined(SearchSysCache1)


PG_FUNCTION_INFO_V1(exec_sql_using);
Datum
exec_sql_using(PG_FUNCTION_ARGS) {
    HeapTuple procedureTuple = SearchSysCache1(PROCOID,
        ObjectIdGetDatum(fcinfo->flinfo->fn_oid));
    if (!HeapTupleIsValid(procedureTuple))
        ereport(ERROR,
            (errmsg("cache lookup failed for function %u",
            fcinfo->flinfo->fn_oid)));

    Oid* types = NULL;
    char** names = NULL;
    char* modes = NULL;
    int nargs = get_func_arg_info(procedureTuple, &types, &names, &modes);

    Oid resultTypeOid;
    TupleDesc tupleDesc;
    TypeFuncClass resultType = get_call_result_type(fcinfo, &resultTypeOid,
        &tupleDesc);
    bool returnTypeIsByValue;
    int16 returnTypeLen;
    get_typlenbyval(resultTypeOid, &returnTypeLen, &returnTypeIsByValue);

    if (resultType != TYPEFUNC_SCALAR && resultType != TYPEFUNC_COMPOSITE)
        ereport(ERROR, (
            errmsg("function \"%s\" has indeterminable result type",
                format_procedure(fcinfo->flinfo->fn_oid))
            ));
    bool returnVoid = resultTypeOid == VOIDOID;

    ReleaseSysCache(procedureTuple);

    if (nargs < 2)
        ereport(ERROR, (
            errmsg("function \"%s\" has less than 2 arguments",
                format_procedure(fcinfo->flinfo->fn_oid))
            ));
    else if (modes != NULL)
        for (int i = 0; i < nargs; i++) {
            if (modes[i] != PROARGMODE_IN)
                ereport(ERROR, (
                    errmsg("function \"%s\" has non-IN arguments",
                        format_procedure(fcinfo->flinfo->fn_oid))
                    ));
        }
    else if (PG_ARGISNULL(0))
        ereport(ERROR, (
            errmsg("function \"%s\" called with NULL as first argument",
                format_procedure(fcinfo->flinfo->fn_oid))
            ));

    char* stmt = NULL;
    if (types[0] == TEXTOID)
        stmt = DatumGetCString(
            DirectFunctionCall1(textout, PG_GETARG_DATUM(0)));
    else if (types[0] == VARCHAROID)
        stmt = DatumGetCString(
            DirectFunctionCall1(varcharout, PG_GETARG_DATUM(0)));
    else
        ereport(ERROR, (
            errmsg("function \"%s\" does not have a leading VARCHAR/TEXT "
                "argument",
                format_procedure(fcinfo->flinfo->fn_oid))
            ));

    char* nulls = NULL;
    for (int i = 1; i < nargs; i++)
        if (PG_ARGISNULL(i)) {
            if (nulls == NULL) {
                nulls = palloc0(sizeof(char) * (nargs - 1));
                memset(nulls, ' ', nargs - 1);
            }
            nulls[i - 1] = 'n';
        }

    SPI_connect();
    SPIPlanPtr plan = SPI_prepare(stmt, nargs - 1, &types[1]);
    if (plan == NULL)
        ereport(ERROR, (
            errmsg("function \"%s\" could not obtain execution plan for "
                "SQL statement",
                format_procedure(fcinfo->flinfo->fn_oid))
            ));

    int result = SPI_execute_plan(plan, &fcinfo->arg[1], nulls, false,
        returnVoid ? 0 : 1);

    Datum returnValue = 0;
    bool returnNull = false;
    if (!returnVoid) {
        if (result != SPI_OK_SELECT
            && result != SPI_OK_INSERT_RETURNING
            && result != SPI_OK_DELETE_RETURNING
            && result == SPI_OK_UPDATE_RETURNING)
            ereport(ERROR, (
                errmsg("function \"%s\" could not obtain result from query",
                    format_procedure(fcinfo->flinfo->fn_oid))
                ));
        else if (SPI_tuptable->tupdesc->natts != 1)
            ereport(ERROR, (
                errmsg("function \"%s\" retrieved more than one column from "
                    "query",
                    format_procedure(fcinfo->flinfo->fn_oid))
                ));
        else if (resultTypeOid != SPI_gettypeid(SPI_tuptable->tupdesc, 1))
            ereport(ERROR, (
                errmsg("function \"%s\" has different return type OID than "
                    "what query returned",
                    format_procedure(fcinfo->flinfo->fn_oid))
                ));

        /* It is important to copy the value into the upper executor context,
         * i.e., the memory context that was current when SPI_connect was
         * called */
        returnValue = SPI_getbinval(SPI_copytuple(SPI_tuptable->vals[0]),
            SPI_tuptable->tupdesc, 1, &returnNull);
    }

    SPI_freeplan(plan);
    if (nulls)
        pfree(nulls);
    SPI_finish();

    if (result < 0)
        ereport(ERROR, (
            errmsg("function \"%s\" encountered error %d during SQL execution",
                format_procedure(fcinfo->flinfo->fn_oid),
                result)
            ));

    if (returnVoid)
        PG_RETURN_VOID();
    else if (returnNull)
        PG_RETURN_NULL();
    else
        return returnValue;
}
