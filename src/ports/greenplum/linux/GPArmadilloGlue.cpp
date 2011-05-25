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

#include <stdexcept>
#include <armadillo>

// System headers
#include <dlfcn.h>

// PostgreSQL headers
#include <utils/elog.h>

#define MADLIB_FORTRAN_FUNCTIONS \
    MADLIB_FORTRAN( sgetrf ) \
    MADLIB_FORTRAN( dgetrf ) \
    MADLIB_FORTRAN( cgetrf ) \
    MADLIB_FORTRAN( zgetrf ) \
    MADLIB_FORTRAN( sgetri ) \
    MADLIB_FORTRAN( dgetri ) \
    MADLIB_FORTRAN( cgetri ) \
    MADLIB_FORTRAN( zgetri ) \
    MADLIB_FORTRAN( strtri ) \
    MADLIB_FORTRAN( dtrtri ) \
    MADLIB_FORTRAN( ctrtri ) \
    MADLIB_FORTRAN( ztrtri ) \
    MADLIB_FORTRAN( ssyev  ) \
    MADLIB_FORTRAN( dsyev  ) \
    MADLIB_FORTRAN( cheev  ) \
    MADLIB_FORTRAN( zheev  ) \
    MADLIB_FORTRAN( sgeev  ) \
    MADLIB_FORTRAN( dgeev  ) \
    MADLIB_FORTRAN( cgeev  ) \
    MADLIB_FORTRAN( zgeev  ) \
    MADLIB_FORTRAN( spotrf ) \
    MADLIB_FORTRAN( dpotrf ) \
    MADLIB_FORTRAN( cpotrf ) \
    MADLIB_FORTRAN( zpotrf ) \
    MADLIB_FORTRAN( sgeqrf ) \
    MADLIB_FORTRAN( dgeqrf ) \
    MADLIB_FORTRAN( cgeqrf ) \
    MADLIB_FORTRAN( zgeqrf ) \
    MADLIB_FORTRAN( sorgqr ) \
    MADLIB_FORTRAN( dorgqr ) \
    MADLIB_FORTRAN( cungqr ) \
    MADLIB_FORTRAN( zungqr ) \
    MADLIB_FORTRAN( sgesvd ) \
    MADLIB_FORTRAN( dgesvd ) \
    MADLIB_FORTRAN( cgesvd ) \
    MADLIB_FORTRAN( zgesvd ) \
    MADLIB_FORTRAN( sgesv  ) \
    MADLIB_FORTRAN( dgesv  ) \
    MADLIB_FORTRAN( cgesv  ) \
    MADLIB_FORTRAN( zgesv  ) \
    MADLIB_FORTRAN( sgels  ) \
    MADLIB_FORTRAN( dgels  ) \
    MADLIB_FORTRAN( cgels  ) \
    MADLIB_FORTRAN( zgels  ) \
    MADLIB_FORTRAN( strtrs ) \
    MADLIB_FORTRAN( dtrtrs ) \
    MADLIB_FORTRAN( ctrtrs ) \
    MADLIB_FORTRAN( ztrtrs ) \
    MADLIB_FORTRAN( sdot   ) \
    MADLIB_FORTRAN( ddot   ) \
    MADLIB_FORTRAN( sgemv  ) \
    MADLIB_FORTRAN( dgemv  ) \
    MADLIB_FORTRAN( cgemv  ) \
    MADLIB_FORTRAN( zgemv  ) \
    MADLIB_FORTRAN( sgemm  ) \
    MADLIB_FORTRAN( dgemm  ) \
    MADLIB_FORTRAN( cgemm  ) \
    MADLIB_FORTRAN( zgemm  )

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



/*


#define MADLIB_FORTRAN(function) \
    extern "C" { \
        __typeof__(arma_fortran(arma_##function)) madlib_##function { \
            static __typeof__(arma_fortran(arma_##function)) *f = NULL; \
            if (f == NULL) \
                f = reinterpret_cast<__typeof__(arma_fortran(arma_##function)) *>( \
                    getFnHandle(#arma_fortran(arma_##function)) \
                ); \
            return call( (*f), fcinfo); \
        } \
    }

*/

static void *getFnHandle(const char *inFnName) {
    if (sHandleLibArmadillo == NULL)
        throw std::runtime_error("libarmadillo.so not found. MADlib will not work properly.");
    
    dlerror();
    void *fnHandle = dlsym(sHandleLibArmadillo, inFnName);
    char *error = dlerror();
    if (error != NULL)
        throw std::runtime_error("libarmadillo.so not found.");
    
    return fnHandle;
}

#define STRINGIFY_INTERMEDIATE(s) #s
#define STRINGIFY(s) STRINGIFY_INTERMEDIATE(s)

#define madlib_fortran(function) \
    static __typeof__(arma_fortran(function)) *f = NULL; \
    if (f == NULL) { \
        f = reinterpret_cast<__typeof__(arma_fortran(function)) *>( \
            getFnHandle(STRINGIFY(arma_fortran(function))) \
        ); \
    } \
    (*f)

using namespace arma;

extern "C" {

// Armadillo LAPACK functions

// LU factorisation
void arma_fortran(arma_sgetrf)(blas_int* m, blas_int* n,  float* a, blas_int* lda, blas_int* ipiv, blas_int* info)
{  madlib_fortran(arma_sgetrf)(          m,           n,         a,           lda,           ipiv,           info);  }
void arma_fortran(arma_dgetrf)(blas_int* m, blas_int* n, double* a, blas_int* lda, blas_int* ipiv, blas_int* info)
{  madlib_fortran(arma_dgetrf)(          m,           n,         a,           lda,           ipiv,           info);  }
void arma_fortran(arma_cgetrf)(blas_int* m, blas_int* n,   void* a, blas_int* lda, blas_int* ipiv, blas_int* info)
{  madlib_fortran(arma_cgetrf)(          m,           n,         a,           lda,           ipiv,           info);  }
void arma_fortran(arma_zgetrf)(blas_int* m, blas_int* n,   void* a, blas_int* lda, blas_int* ipiv, blas_int* info)
{  madlib_fortran(arma_zgetrf)(          m,           n,         a,           lda,           ipiv,           info);  }

// matrix inversion (using LU factorisation result)
void arma_fortran(arma_sgetri)(blas_int* n,  float* a, blas_int* lda, blas_int* ipiv,  float* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_sgetri)(          n,         a,           lda,           ipiv,         work,           lwork,           info);  }
void arma_fortran(arma_dgetri)(blas_int* n, double* a, blas_int* lda, blas_int* ipiv, double* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_dgetri)(          n,         a,           lda,           ipiv,         work,           lwork,           info);  }
void arma_fortran(arma_cgetri)(blas_int* n,  void*  a, blas_int* lda, blas_int* ipiv,   void* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_cgetri)(          n,         a,           lda,           ipiv,         work,           lwork,           info);  }
void arma_fortran(arma_zgetri)(blas_int* n,  void*  a, blas_int* lda, blas_int* ipiv,   void* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_zgetri)(          n,         a,           lda,           ipiv,         work,           lwork,           info);  }

// matrix inversion (triangular matrices)
void arma_fortran(arma_strtri)(char* uplo, char* diag, blas_int* n,  float* a, blas_int* lda, blas_int* info)
{  madlib_fortran(arma_strtri)(      uplo,       diag,           n,         a,           lda,           info);  }
void arma_fortran(arma_dtrtri)(char* uplo, char* diag, blas_int* n, double* a, blas_int* lda, blas_int* info)
{  madlib_fortran(arma_dtrtri)(      uplo,       diag,           n,         a,           lda,           info);  }
void arma_fortran(arma_ctrtri)(char* uplo, char* diag, blas_int* n,   void* a, blas_int* lda, blas_int* info)
{  madlib_fortran(arma_ctrtri)(      uplo,       diag,           n,         a,           lda,           info);  }
void arma_fortran(arma_ztrtri)(char* uplo, char* diag, blas_int* n,   void* a, blas_int* lda, blas_int* info)
{  madlib_fortran(arma_ztrtri)(      uplo,       diag,           n,         a,           lda,           info);  }

// eigenvector decomposition of symmetric real matrices
void arma_fortran(arma_ssyev)(char* jobz, char* uplo, blas_int* n,  float* a, blas_int* lda,  float* w,  float* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_ssyev)(      jobz,       uplo,           n,         a,           lda,         w,         work,           lwork,           info);  }
void arma_fortran(arma_dsyev)(char* jobz, char* uplo, blas_int* n, double* a, blas_int* lda, double* w, double* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_dsyev)(      jobz,       uplo,           n,         a,           lda,         w,         work,           lwork,           info);  }

// eigenvector decomposition of hermitian matrices (complex)
void arma_fortran(arma_cheev)(char* jobz, char* uplo, blas_int* n,   void* a, blas_int* lda,  float* w,   void* work, blas_int* lwork,  float* rwork, blas_int* info)
{  madlib_fortran(arma_cheev)(      jobz,       uplo,           n,         a,           lda,         w,         work,           lwork,         rwork,           info);  }
void arma_fortran(arma_zheev)(char* jobz, char* uplo, blas_int* n,   void* a, blas_int* lda, double* w,   void* work, blas_int* lwork, double* rwork, blas_int* info)
{  madlib_fortran(arma_zheev)(      jobz,       uplo,           n,         a,           lda,         w,         work,           lwork,         rwork,           info);  }

// eigenvector decomposition of general real matrices
void arma_fortran(arma_sgeev)(char* jobvl, char* jobvr, blas_int* n,  float* a, blas_int* lda,  float* wr,  float* wi,  float* vl, blas_int* ldvl,  float* vr, blas_int* ldvr,  float* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_sgeev)(      jobvl,       jobvr,           n,         a,           lda,         wr,         wi,         vl,           ldvl,         vr,           ldvr,         work,           lwork,           info);  }
void arma_fortran(arma_dgeev)(char* jobvl, char* jobvr, blas_int* n, double* a, blas_int* lda, double* wr, double* wi, double* vl, blas_int* ldvl, double* vr, blas_int* ldvr, double* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_dgeev)(      jobvl,       jobvr,           n,         a,           lda,         wr,         wi,         vl,           ldvl,         vr,           ldvr,         work,           lwork,           info);  }

// eigenvector decomposition of general complex matrices
void arma_fortran(arma_cgeev)(char* jobvr, char* jobvl, blas_int* n, void* a, blas_int* lda, void* w, void* vl, blas_int* ldvl, void* vr, blas_int* ldvr, void* work, blas_int* lwork,  float* rwork, blas_int* info)
{  madlib_fortran(arma_cgeev)(      jobvr,       jobvl,           n,       a,           lda,       w,       vl,           ldvl,       vr,           ldvr,       work,           lwork,         rwork,           info);  }
void arma_fortran(arma_zgeev)(char* jobvl, char* jobvr, blas_int* n, void* a, blas_int* lda, void* w, void* vl, blas_int* ldvl, void* vr, blas_int* ldvr, void* work, blas_int* lwork, double* rwork, blas_int* info)
{  madlib_fortran(arma_zgeev)(      jobvr,       jobvl,           n,       a,           lda,       w,       vl,           ldvl,       vr,           ldvr,       work,           lwork,         rwork,           info);  }

// Cholesky decomposition
void arma_fortran(arma_spotrf)(char* uplo, blas_int* n,  float* a, blas_int* lda, blas_int* info)
{  madlib_fortran(arma_spotrf)(      uplo,           n,         a,           lda,           info);  }
void arma_fortran(arma_dpotrf)(char* uplo, blas_int* n, double* a, blas_int* lda, blas_int* info)
{  madlib_fortran(arma_dpotrf)(      uplo,           n,         a,           lda,           info);  }
void arma_fortran(arma_cpotrf)(char* uplo, blas_int* n,   void* a, blas_int* lda, blas_int* info)
{  madlib_fortran(arma_cpotrf)(      uplo,           n,         a,           lda,           info);  }
void arma_fortran(arma_zpotrf)(char* uplo, blas_int* n,   void* a, blas_int* lda, blas_int* info)
{  madlib_fortran(arma_zpotrf)(      uplo,           n,         a,           lda,           info);  }

// QR decomposition
void arma_fortran(arma_sgeqrf)(blas_int* m, blas_int* n,  float* a, blas_int* lda,  float* tau,  float* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_sgeqrf)(          m,           n,         a,           lda,         tau,         work,           lwork,           info);  }
void arma_fortran(arma_dgeqrf)(blas_int* m, blas_int* n, double* a, blas_int* lda, double* tau, double* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_dgeqrf)(          m,           n,         a,           lda,         tau,         work,           lwork,           info);  }
void arma_fortran(arma_cgeqrf)(blas_int* m, blas_int* n,   void* a, blas_int* lda,   void* tau,   void* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_cgeqrf)(          m,           n,         a,           lda,         tau,         work,           lwork,           info);  }
void arma_fortran(arma_zgeqrf)(blas_int* m, blas_int* n,   void* a, blas_int* lda,   void* tau,   void* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_zgeqrf)(          m,           n,         a,           lda,         tau,         work,           lwork,           info);  }

// Q matrix calculation from QR decomposition (real matrices)
void arma_fortran(arma_sorgqr)(blas_int* m, blas_int* n, blas_int* k,  float* a, blas_int* lda,  float* tau,  float* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_sorgqr)(          m,           n,           k,         a,           lda,         tau,         work,           lwork,           info);  }
void arma_fortran(arma_dorgqr)(blas_int* m, blas_int* n, blas_int* k, double* a, blas_int* lda, double* tau, double* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_dorgqr)(          m,           n,           k,         a,           lda,         tau,         work,           lwork,           info);  }

// Q matrix calculation from QR decomposition (complex matrices)
void arma_fortran(arma_cungqr)(blas_int* m, blas_int* n, blas_int* k,   void* a, blas_int* lda,   void* tau,   void* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_cungqr)(          m,           n,           k,         a,           lda,         tau,         work,           lwork,           info);  }
void arma_fortran(arma_zungqr)(blas_int* m, blas_int* n, blas_int* k,   void* a, blas_int* lda,   void* tau,   void* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_zungqr)(          m,           n,           k,         a,           lda,         tau,         work,           lwork,           info);  }

// SVD (real matrices)
void arma_fortran(arma_sgesvd)(char* jobu, char* jobvt, blas_int* m, blas_int* n, float*  a, blas_int* lda, float*  s, float*  u, blas_int* ldu, float*  vt, blas_int* ldvt, float*  work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_sgesvd)(      jobu,       jobvt,           m,           n,         a,           lda,         s,         u,           ldu,         vt,           ldvt,         work,           lwork,           info);  }
void arma_fortran(arma_dgesvd)(char* jobu, char* jobvt, blas_int* m, blas_int* n, double* a, blas_int* lda, double* s, double* u, blas_int* ldu, double* vt, blas_int* ldvt, double* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_dgesvd)(      jobu,       jobvt,           m,           n,         a,           lda,         s,         u,           ldu,         vt,           ldvt,         work,           lwork,           info);  }

// SVD (complex matrices)
void arma_fortran(arma_cgesvd)(char* jobu, char* jobvt, blas_int* m, blas_int* n, void*   a, blas_int* lda, float*  s, void*   u, blas_int* ldu, void*   vt, blas_int* ldvt, void*   work, blas_int* lwork, float*  rwork, blas_int* info)
{  madlib_fortran(arma_cgesvd)(      jobu,       jobvt,           m,           n,         a,           lda,         s,         u,           ldu,         vt,           ldvt,         work,           lwork,         rwork,           info);  }
void arma_fortran(arma_zgesvd)(char* jobu, char* jobvt, blas_int* m, blas_int* n, void*   a, blas_int* lda, double* s, void*   u, blas_int* ldu, void*   vt, blas_int* ldvt, void*   work, blas_int* lwork, double* rwork, blas_int* info)
{  madlib_fortran(arma_zgesvd)(      jobu,       jobvt,           m,           n,         a,           lda,         s,         u,           ldu,         vt,           ldvt,         work,           lwork,         rwork,           info);  }

// solve system of linear equations, using LU decomposition
void arma_fortran(arma_sgesv)(blas_int* n, blas_int* nrhs, float*  a, blas_int* lda, blas_int* ipiv, float*  b, blas_int* ldb, blas_int* info)
{  madlib_fortran(arma_sgesv)(          n,           nrhs,         a,           lda,           ipiv,         b,           ldb,           info);  }
void arma_fortran(arma_dgesv)(blas_int* n, blas_int* nrhs, double* a, blas_int* lda, blas_int* ipiv, double* b, blas_int* ldb, blas_int* info)
{  madlib_fortran(arma_dgesv)(          n,           nrhs,         a,           lda,           ipiv,         b,           ldb,           info);  }
void arma_fortran(arma_cgesv)(blas_int* n, blas_int* nrhs, void*   a, blas_int* lda, blas_int* ipiv, void*   b, blas_int* ldb, blas_int* info)
{  madlib_fortran(arma_cgesv)(          n,           nrhs,         a,           lda,           ipiv,         b,           ldb,           info);  }
void arma_fortran(arma_zgesv)(blas_int* n, blas_int* nrhs, void*   a, blas_int* lda, blas_int* ipiv, void*   b, blas_int* ldb, blas_int* info)
{  madlib_fortran(arma_zgesv)(          n,           nrhs,         a,           lda,           ipiv,         b,           ldb,           info);  }

// solve over/underdetermined system of linear equations
void arma_fortran(arma_sgels)(char* trans, blas_int* m, blas_int* n, blas_int* nrhs, float*  a, blas_int* lda, float*  b, blas_int* ldb, float*  work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_sgels)(      trans,           m,           n,           nrhs,         a,           lda,         b,           ldb,         work,           lwork,           info);  }
void arma_fortran(arma_dgels)(char* trans, blas_int* m, blas_int* n, blas_int* nrhs, double* a, blas_int* lda, double* b, blas_int* ldb, double* work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_dgels)(      trans,           m,           n,           nrhs,         a,           lda,         b,           ldb,         work,           lwork,           info);  }
void arma_fortran(arma_cgels)(char* trans, blas_int* m, blas_int* n, blas_int* nrhs, void*   a, blas_int* lda, void*   b, blas_int* ldb, void*   work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_cgels)(      trans,           m,           n,           nrhs,         a,           lda,         b,           ldb,         work,           lwork,           info);  }
void arma_fortran(arma_zgels)(char* trans, blas_int* m, blas_int* n, blas_int* nrhs, void*   a, blas_int* lda, void*   b, blas_int* ldb, void*   work, blas_int* lwork, blas_int* info)
{  madlib_fortran(arma_zgels)(      trans,           m,           n,           nrhs,         a,           lda,         b,           ldb,         work,           lwork,           info);  }

// solve a triangular system of linear equations
void arma_fortran(arma_strtrs)(char* uplo, char* trans, char* diag, blas_int* n, blas_int* nrhs, const float*  a, blas_int* lda, float*  b, blas_int* ldb, blas_int* info)
{  madlib_fortran(arma_strtrs)(      uplo,       trans,       diag,           n,           nrhs,           lda,             lda,         b,           ldb,           info);  }
void arma_fortran(arma_dtrtrs)(char* uplo, char* trans, char* diag, blas_int* n, blas_int* nrhs, const double* a, blas_int* lda, double* b, blas_int* ldb, blas_int* info)
{  madlib_fortran(arma_dtrtrs)(      uplo,       trans,       diag,           n,           nrhs,           lda,             lda,         b,           ldb,           info);  }
void arma_fortran(arma_ctrtrs)(char* uplo, char* trans, char* diag, blas_int* n, blas_int* nrhs, const void*   a, blas_int* lda, void*   b, blas_int* ldb, blas_int* info)
{  madlib_fortran(arma_ctrtrs)(      uplo,       trans,       diag,           n,           nrhs,           lda,             lda,         b,           ldb,           info);  }
void arma_fortran(arma_ztrtrs)(char* uplo, char* trans, char* diag, blas_int* n, blas_int* nrhs, const void*   a, blas_int* lda, void*   b, blas_int* ldb, blas_int* info)
{  madlib_fortran(arma_ztrtrs)(      uplo,       trans,       diag,           n,           nrhs,           lda,             lda,         b,           ldb,           info);  }


// Armadillo BLAS functions

float      arma_fortran(arma_sdot)(blas_int* n, const float*  x, blas_int* incx, const float*  y, blas_int* incy)
{ return madlib_fortran(arma_sdot)(          n,               x,           incx,               y,           incy); }
double     arma_fortran(arma_ddot)(blas_int* n, const double* x, blas_int* incx, const double* y, blas_int* incy)
{ return madlib_fortran(arma_ddot)(          n,               x,           incx,               y,           incy); }

void arma_fortran(arma_sgemv)(const char* transA, const blas_int* m, const blas_int* n, const float*  alpha, const float*  A, const blas_int* ldA, const float*  x, const blas_int* incx, const float*  beta, float*  y, const blas_int* incy)
{  madlib_fortran(arma_sgemv)(            transA,                 m,                 n,               alpha,               A,                 ldA,               x,                 incx,               beta,         y,                 incy); }
void arma_fortran(arma_dgemv)(const char* transA, const blas_int* m, const blas_int* n, const double* alpha, const double* A, const blas_int* ldA, const double* x, const blas_int* incx, const double* beta, double* y, const blas_int* incy)
{  madlib_fortran(arma_dgemv)(            transA,                 m,                 n,               alpha,               A,                 ldA,               x,                 incx,               beta,         y,                 incy); }
void arma_fortran(arma_cgemv)(const char* transA, const blas_int* m, const blas_int* n, const void*   alpha, const void*   A, const blas_int* ldA, const void*   x, const blas_int* incx, const void*   beta, void*   y, const blas_int* incy)
{  madlib_fortran(arma_cgemv)(            transA,                 m,                 n,               alpha,               A,                 ldA,               x,                 incx,               beta,         y,                 incy); }
void arma_fortran(arma_zgemv)(const char* transA, const blas_int* m, const blas_int* n, const void*   alpha, const void*   A, const blas_int* ldA, const void*   x, const blas_int* incx, const void*   beta, void*   y, const blas_int* incy)
{  madlib_fortran(arma_zgemv)(            transA,                 m,                 n,               alpha,               A,                 ldA,               x,                 incx,               beta,         y,                 incy); }

void arma_fortran(arma_sgemm)(const char* transA, const char* transB, const blas_int* m, const blas_int* n, const blas_int* k, const float*  alpha, const float*  A, const blas_int* ldA, const float*  B, const blas_int* ldB, const float*  beta, float*  C, const blas_int* ldC)
{  madlib_fortran(arma_sgemm)(            transA,             transB,                 m,                 n,                 k,               alpha,               A,                 ldA,               B,                 ldB,               beta,         C,                 ldC); }
void arma_fortran(arma_dgemm)(const char* transA, const char* transB, const blas_int* m, const blas_int* n, const blas_int* k, const double* alpha, const double* A, const blas_int* ldA, const double* B, const blas_int* ldB, const double* beta, double* C, const blas_int* ldC)
{  madlib_fortran(arma_dgemm)(            transA,             transB,                 m,                 n,                 k,               alpha,               A,                 ldA,               B,                 ldB,               beta,         C,                 ldC); }
void arma_fortran(arma_cgemm)(const char* transA, const char* transB, const blas_int* m, const blas_int* n, const blas_int* k, const void*   alpha, const void*   A, const blas_int* ldA, const void*   B, const blas_int* ldB, const void*   beta, void*   C, const blas_int* ldC)
{  madlib_fortran(arma_cgemm)(            transA,             transB,                 m,                 n,                 k,               alpha,               A,                 ldA,               B,                 ldB,               beta,         C,                 ldC); }
void arma_fortran(arma_zgemm)(const char* transA, const char* transB, const blas_int* m, const blas_int* n, const blas_int* k, const void*   alpha, const void*   A, const blas_int* ldA, const void*   B, const blas_int* ldB, const void*   beta, void*   C, const blas_int* ldC)
{  madlib_fortran(arma_zgemm)(            transA,             transB,                 m,                 n,                 k,               alpha,               A,                 ldA,               B,                 ldB,               beta,         C,                 ldC); }

} // extern "C"

} // namespace dbconnector

} // namespace madlib
