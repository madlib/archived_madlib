#include <postgres.h>
#include <fmgr.h>
#include <funcapi.h>
#include <catalog/pg_proc.h>
#include <catalog/pg_type.h>
#include <executor/spi.h>
#include <utils/builtins.h>
#include <utils/syscache.h>

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
    
    int result = SPI_execute_plan(plan, &fcinfo->arg[1], nulls, false, 0);
    
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
    
    PG_RETURN_VOID();
}
