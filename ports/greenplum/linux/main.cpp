/* ----------------------------------------------------------------------- *//**
 *
 * @file main.cpp
 *
 * Greenplum <= 4.1 statically links to CLAPACK and BLAS. However, it only
 * contains a subset of these libraries (unused symbols are removed from its
 * postgres binary). This causes major headache when writing compiled UDFs that
 * itself rely on these third-party libraries but need a superset of what is
 * contained in GPDB.
 * 
 * Reason: When GPDB loads the shared library containing the compiled UDF with
 * dlopen, each used symbols is bound to the definition that was loaded
 * first [1].
 *
 * Example: For some functions used by MADlib (such as dlange, which is called
 * from dgesvd, which is called from arma::pinv) the definition first loaded is
 * in the GPDB binary, for some other function (say, dgesvd, which is not
 * contained in the GPDB image) this would be /usr/lib/liblapack.so. Clearly,
 * mixing implementations from potentially different versions of a third-party
 * library almost certainly calls for trouble.
 *
 * In MADlib, we want to dynamically link to the system-provided version of
 * LAPACK and BLAS (because these are probably tuned and optimized). Hence, we
 * have only a lazy link to armadillo and then call dlload explicitly with
 * the RTLD_DEEPBIND option [2] in the second argument. (BLAS and possibly ATLAS
 * will be loaded as depedencies with the same settings.)
 * 
 * This ensures that all calls into the external LAPACK library will not call
 * back into the main image in case when symbols with the same name exist there.
 * (E.g., dgesvd calls dlange, which would exists both in the postgres binary
 * and in the external LAPACK.)
 *
 * [1] POSIX standard on dlopen:
 *     http://pubs.opengroup.org/onlinepubs/9699919799/functions/dlopen.html
 * [2] man dlopen on Linux (since glibc 2.3.4)
 *
 *//* ----------------------------------------------------------------------- */

// MADlib headers
#include <madlib/ports/postgres/main.hpp>
#include <madlib/modules/modules.hpp>

// System headers
#include <dlfcn.h>

// PostgreSQL headers
#include <utils/elog.h>

namespace madlib {

namespace ports {

using namespace postgres;

namespace greenplum {

static void *sHandleMADlib = NULL;

__attribute__((constructor))
void madlib_constructor() {
    dlerror();
    sHandleMADlib = dlopen("libmad.so", RTLD_NOW | RTLD_GLOBAL | RTLD_DEEPBIND);
    if (!sHandleMADlib) {
        ereport(WARNING,
            (errmsg("libmad.so not found. MADlib will not work correctly."),
            errdetail("%s", dlerror())));
    }
}

__attribute__((destructor))
void madlib_destructor() {
    if (sHandleMADlib)
        dlclose(sHandleMADlib);
}

extern "C" {
    PG_MODULE_MAGIC;
} // extern "C"

#define DECLARE_UDF(NameSpace, Function) DECLARE_UDF_EXT(Function, NameSpace, Function)

#define DECLARE_UDF_EXT(SQLName, NameSpace, Function) \
    extern "C" { \
        Datum SQLName(PG_FUNCTION_ARGS); \
        PG_FUNCTION_INFO_V1(SQLName); \
        Datum SQLName(PG_FUNCTION_ARGS) { \
            static MADFunction *f = NULL; \
            if (f == NULL) \
                f = reinterpret_cast<MADFunction*>( \
                    getFnHandle("madlib_" #SQLName, fcinfo) \
                ); \
            return call( (*f), fcinfo); \
        } \
    }
    
static void *getFnHandle(const char *inFnName, PG_FUNCTION_ARGS) {
    if (sHandleMADlib == NULL) {
        ereport(
            ERROR, (
                errmsg( \
                    "Function \"%s\": libmad.so not found. "
                    "MADlib will not work correclty.",
                    format_procedure(fcinfo->flinfo->fn_oid)
                ),
                errhint("The MADlib installation could be broken")
            )
        );
        // This position will not be reached
    }

    dlerror();
    void *fnHandle = dlsym(sHandleMADlib, inFnName);
    char *error = dlerror();
    if (error != NULL) {
        ereport(
            ERROR, (
                errmsg(
                    "Function \"%s\" cannot be found in libmad.so",
                    format_procedure(fcinfo->flinfo->fn_oid)
                ),
                errhint("The MADlib installation could be broken"),
                errdetail("%s", error)
            )
        );
        // This position will not be reached
    }
    
    return fnHandle;
}

#include <madlib/modules/declarations.hpp>

#undef DECLARE_UDF_EXT
#undef DECLARE_UDF

} // namespace greenplum

} // namespace ports

} // namespace madlib
