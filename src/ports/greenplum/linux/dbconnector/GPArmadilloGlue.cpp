/* ----------------------------------------------------------------------- *//**
 *
 * @file GPArmadilloGlue.cpp
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
 * In MADlib, we want to use the system-provided version of LAPACK and BLAS
 * (because these are probably tuned and optimized). One option would be to
 * dynamically load the core library on OR in RTLD_DEEPBIND to the second
 * argument [2]. Unfortunately, this is also a bad idea because, due to the GCC
 * C++ ABI, there would be ugly side-effects on C++ semanticss [3].
 *
 * The solution that we use is to dynamically load armadillo at runtime and OR
 * in RTLD_DEEPBIND to the second argument. (BLAS and possibly ATLAS
 * will be loaded as depedencies with the same settings.) To make this work, the
 * core library calls madlib_arma_<LAPACK-function>, which is defined in the
 * connector library (i.e., here). The connector library looks up
 * <LAPACK-function> with dlsym() within libarmadillo.so and its dependencies
 * (i.e., LAPACK/BLAS).
 * 
 * The RTLD_DEEPBIND ensures that all calls into the external LAPACK library
 * will not call back into the main image in case when symbols with the same
 * name exist there. (E.g., dgesvd calls dlange, which would exists both in the
 * postgres binary and in the external LAPACK.)
 *
 * [1] POSIX standard on dlopen:
 *     http://pubs.opengroup.org/onlinepubs/9699919799/functions/dlopen.html
 * [2] man dlopen on Linux (since glibc 2.3.4)
 * [3] http://gcc.gnu.org/faq.html#dso
 *
 *//* ----------------------------------------------------------------------- */

#include <stdexcept>
#include <armadillo>

// System headers
#include <dlfcn.h>

extern "C" {
    // PostgreSQL headers
    #include <utils/elog.h>
} // extern "C"

namespace madlib {

namespace dbconnector {

static void *sHandleLibArmadillo = NULL;

static void *ArmadilloNotFound() {
    throw std::runtime_error("libarmadillo.so not found.");
}

__attribute__((constructor))
void madlib_constructor() {
    dlerror();
    // FIXME: Think again about RTLD_GLOBAL and what happens if other UDFs
    // depend on LAPACK/BLAS
    sHandleLibArmadillo = dlopen("libarmadillo.so", RTLD_NOW | RTLD_DEEPBIND);
    if (!sHandleLibArmadillo) {
        ereport(WARNING,
            (errmsg("libarmadillo.so not found. MADlib will not work correctly."),
            errdetail("%s", dlerror())));
    }
}

__attribute__((destructor))
void madlib_destructor() {
    if (sHandleLibArmadillo)
        dlclose(sHandleLibArmadillo);
}

static void *getFnHandle(const char *inFnName) {
    if (sHandleLibArmadillo == NULL)
        throw std::runtime_error("libarmadillo.so not found.");
    
    dlerror();
    void *fnHandle = dlsym(sHandleLibArmadillo, inFnName);
    char *error = dlerror();
    if (error != NULL)
        throw std::runtime_error("Could not find function in libarmadillo.so.");
    
    return fnHandle;
}

#define STRINGIFY_INTERMEDIATE(s) #s
#define STRINGIFY(s) STRINGIFY_INTERMEDIATE(s)

#define MADLIB_FORTRAN_INIT(madlib_function, function) \
    static __typeof__(madlib_function) *f = NULL; \
    if (f == NULL) { \
        f = reinterpret_cast<__typeof__(madlib_function) *>( \
            getFnHandle(STRINGIFY(arma_fortran2(function))) \
        ); \
    }

#define MADLIB_FORTRAN_QUALIFIER

#define MADLIB_FORTRAN_DECLARE(x) { x; }

#define MADLIB_FORTRAN(function) \
    MADLIB_FORTRAN_INIT(madlib_##function, function) \
    (*f)

#define MADLIB_FORTRAN_RETURN(function) \
    MADLIB_FORTRAN_INIT(madlib_##function, function) \
    return (*f)


using namespace arma;

extern "C" {

#include <dbconnector/ArmadilloDeclarations.hpp>

} // extern "C"

} // namespace dbconnector

} // namespace madlib
