#include <postgres.h>
#include <fmgr.h>

PG_FUNCTION_INFO_V1(noop);
Datum
noop(PG_FUNCTION_ARGS) {
    (void) fcinfo;
    PG_RETURN_VOID();
}
