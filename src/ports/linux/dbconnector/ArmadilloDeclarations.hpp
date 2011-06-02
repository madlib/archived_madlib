/* ----------------------------------------------------------------------- *//**
 *
 * @file ArmadilloDeclarations.hpp
 *
 * On Linux, the connector library has to provide
 * madlib_arma_<LAPACK/BLAS function name>, see
 * ports/greenplum/linux/GPArmadilloGlue.cpp for more background.
 *
 *//* ----------------------------------------------------------------------- */

// Armadillo LAPACK functions

// LU factorisation
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_sgetrf)(blas_int* m, blas_int* n,  float* a, blas_int* lda, blas_int* ipiv, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_sgetrf)(          m,           n,         a,           lda,           ipiv,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dgetrf)(blas_int* m, blas_int* n, double* a, blas_int* lda, blas_int* ipiv, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dgetrf)(          m,           n,         a,           lda,           ipiv,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_cgetrf)(blas_int* m, blas_int* n,   void* a, blas_int* lda, blas_int* ipiv, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_cgetrf)(          m,           n,         a,           lda,           ipiv,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_zgetrf)(blas_int* m, blas_int* n,   void* a, blas_int* lda, blas_int* ipiv, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_zgetrf)(          m,           n,         a,           lda,           ipiv,           info)  )

// matrix inversion (using LU factorisation result)
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_sgetri)(blas_int* n,  float* a, blas_int* lda, blas_int* ipiv,  float* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_sgetri)(          n,         a,           lda,           ipiv,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dgetri)(blas_int* n, double* a, blas_int* lda, blas_int* ipiv, double* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dgetri)(          n,         a,           lda,           ipiv,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_cgetri)(blas_int* n,  void*  a, blas_int* lda, blas_int* ipiv,   void* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_cgetri)(          n,         a,           lda,           ipiv,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_zgetri)(blas_int* n,  void*  a, blas_int* lda, blas_int* ipiv,   void* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_zgetri)(          n,         a,           lda,           ipiv,         work,           lwork,           info)  )

// matrix inversion (triangular matrices)
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_strtri)(char* uplo, char* diag, blas_int* n,  float* a, blas_int* lda, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_strtri)(      uplo,       diag,           n,         a,           lda,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dtrtri)(char* uplo, char* diag, blas_int* n, double* a, blas_int* lda, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dtrtri)(      uplo,       diag,           n,         a,           lda,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_ctrtri)(char* uplo, char* diag, blas_int* n,   void* a, blas_int* lda, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_ctrtri)(      uplo,       diag,           n,         a,           lda,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_ztrtri)(char* uplo, char* diag, blas_int* n,   void* a, blas_int* lda, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_ztrtri)(      uplo,       diag,           n,         a,           lda,           info)  )

// eigenvector decomposition of symmetric real matrices
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_ssyev)(char* jobz, char* uplo, blas_int* n,  float* a, blas_int* lda,  float* w,  float* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_ssyev)(      jobz,       uplo,           n,         a,           lda,         w,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dsyev)(char* jobz, char* uplo, blas_int* n, double* a, blas_int* lda, double* w, double* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dsyev)(      jobz,       uplo,           n,         a,           lda,         w,         work,           lwork,           info)  )

// eigenvector decomposition of hermitian matrices (complex)
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_cheev)(char* jobz, char* uplo, blas_int* n,   void* a, blas_int* lda,  float* w,   void* work, blas_int* lwork,  float* rwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_cheev)(      jobz,       uplo,           n,         a,           lda,         w,         work,           lwork,         rwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_zheev)(char* jobz, char* uplo, blas_int* n,   void* a, blas_int* lda, double* w,   void* work, blas_int* lwork, double* rwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_zheev)(      jobz,       uplo,           n,         a,           lda,         w,         work,           lwork,         rwork,           info)  )

// eigenvector decomposition of general real matrices
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_sgeev)(char* jobvl, char* jobvr, blas_int* n,  float* a, blas_int* lda,  float* wr,  float* wi,  float* vl, blas_int* ldvl,  float* vr, blas_int* ldvr,  float* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_sgeev)(      jobvl,       jobvr,           n,         a,           lda,         wr,         wi,         vl,           ldvl,         vr,           ldvr,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dgeev)(char* jobvl, char* jobvr, blas_int* n, double* a, blas_int* lda, double* wr, double* wi, double* vl, blas_int* ldvl, double* vr, blas_int* ldvr, double* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dgeev)(      jobvl,       jobvr,           n,         a,           lda,         wr,         wi,         vl,           ldvl,         vr,           ldvr,         work,           lwork,           info)  )

// eigenvector decomposition of general complex matrices
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_cgeev)(char* jobvr, char* jobvl, blas_int* n, void* a, blas_int* lda, void* w, void* vl, blas_int* ldvl, void* vr, blas_int* ldvr, void* work, blas_int* lwork,  float* rwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_cgeev)(      jobvr,       jobvl,           n,       a,           lda,       w,       vl,           ldvl,       vr,           ldvr,       work,           lwork,         rwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_zgeev)(char* jobvl, char* jobvr, blas_int* n, void* a, blas_int* lda, void* w, void* vl, blas_int* ldvl, void* vr, blas_int* ldvr, void* work, blas_int* lwork, double* rwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_zgeev)(      jobvr,       jobvl,           n,       a,           lda,       w,       vl,           ldvl,       vr,           ldvr,       work,           lwork,         rwork,           info)  )

// Cholesky decomposition
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_spotrf)(char* uplo, blas_int* n,  float* a, blas_int* lda, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_spotrf)(      uplo,           n,         a,           lda,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dpotrf)(char* uplo, blas_int* n, double* a, blas_int* lda, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dpotrf)(      uplo,           n,         a,           lda,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_cpotrf)(char* uplo, blas_int* n,   void* a, blas_int* lda, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_cpotrf)(      uplo,           n,         a,           lda,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_zpotrf)(char* uplo, blas_int* n,   void* a, blas_int* lda, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_zpotrf)(      uplo,           n,         a,           lda,           info)  )

// QR decomposition
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_sgeqrf)(blas_int* m, blas_int* n,  float* a, blas_int* lda,  float* tau,  float* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_sgeqrf)(          m,           n,         a,           lda,         tau,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dgeqrf)(blas_int* m, blas_int* n, double* a, blas_int* lda, double* tau, double* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dgeqrf)(          m,           n,         a,           lda,         tau,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_cgeqrf)(blas_int* m, blas_int* n,   void* a, blas_int* lda,   void* tau,   void* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_cgeqrf)(          m,           n,         a,           lda,         tau,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_zgeqrf)(blas_int* m, blas_int* n,   void* a, blas_int* lda,   void* tau,   void* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_zgeqrf)(          m,           n,         a,           lda,         tau,         work,           lwork,           info)  )

// Q matrix calculation from QR decomposition (real matrices)
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_sorgqr)(blas_int* m, blas_int* n, blas_int* k,  float* a, blas_int* lda,  float* tau,  float* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_sorgqr)(          m,           n,           k,         a,           lda,         tau,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dorgqr)(blas_int* m, blas_int* n, blas_int* k, double* a, blas_int* lda, double* tau, double* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dorgqr)(          m,           n,           k,         a,           lda,         tau,         work,           lwork,           info)  )

// Q matrix calculation from QR decomposition (complex matrices)
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_cungqr)(blas_int* m, blas_int* n, blas_int* k,   void* a, blas_int* lda,   void* tau,   void* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_cungqr)(          m,           n,           k,         a,           lda,         tau,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_zungqr)(blas_int* m, blas_int* n, blas_int* k,   void* a, blas_int* lda,   void* tau,   void* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_zungqr)(          m,           n,           k,         a,           lda,         tau,         work,           lwork,           info)  )

// SVD (real matrices)
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_sgesvd)(char* jobu, char* jobvt, blas_int* m, blas_int* n, float*  a, blas_int* lda, float*  s, float*  u, blas_int* ldu, float*  vt, blas_int* ldvt, float*  work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_sgesvd)(      jobu,       jobvt,           m,           n,         a,           lda,         s,         u,           ldu,         vt,           ldvt,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dgesvd)(char* jobu, char* jobvt, blas_int* m, blas_int* n, double* a, blas_int* lda, double* s, double* u, blas_int* ldu, double* vt, blas_int* ldvt, double* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dgesvd)(      jobu,       jobvt,           m,           n,         a,           lda,         s,         u,           ldu,         vt,           ldvt,         work,           lwork,           info)  )

// SVD (complex matrices)
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_cgesvd)(char* jobu, char* jobvt, blas_int* m, blas_int* n, void*   a, blas_int* lda, float*  s, void*   u, blas_int* ldu, void*   vt, blas_int* ldvt, void*   work, blas_int* lwork, float*  rwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_cgesvd)(      jobu,       jobvt,           m,           n,         a,           lda,         s,         u,           ldu,         vt,           ldvt,         work,           lwork,         rwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_zgesvd)(char* jobu, char* jobvt, blas_int* m, blas_int* n, void*   a, blas_int* lda, double* s, void*   u, blas_int* ldu, void*   vt, blas_int* ldvt, void*   work, blas_int* lwork, double* rwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_zgesvd)(      jobu,       jobvt,           m,           n,         a,           lda,         s,         u,           ldu,         vt,           ldvt,         work,           lwork,         rwork,           info)  )

// solve system of linear equations, using LU decomposition
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_sgesv)(blas_int* n, blas_int* nrhs, float*  a, blas_int* lda, blas_int* ipiv, float*  b, blas_int* ldb, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_sgesv)(          n,           nrhs,         a,           lda,           ipiv,         b,           ldb,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dgesv)(blas_int* n, blas_int* nrhs, double* a, blas_int* lda, blas_int* ipiv, double* b, blas_int* ldb, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dgesv)(          n,           nrhs,         a,           lda,           ipiv,         b,           ldb,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_cgesv)(blas_int* n, blas_int* nrhs, void*   a, blas_int* lda, blas_int* ipiv, void*   b, blas_int* ldb, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_cgesv)(          n,           nrhs,         a,           lda,           ipiv,         b,           ldb,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_zgesv)(blas_int* n, blas_int* nrhs, void*   a, blas_int* lda, blas_int* ipiv, void*   b, blas_int* ldb, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_zgesv)(          n,           nrhs,         a,           lda,           ipiv,         b,           ldb,           info)  )

// solve over/underdetermined system of linear equations
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_sgels)(char* trans, blas_int* m, blas_int* n, blas_int* nrhs, float*  a, blas_int* lda, float*  b, blas_int* ldb, float*  work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_sgels)(      trans,           m,           n,           nrhs,         a,           lda,         b,           ldb,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dgels)(char* trans, blas_int* m, blas_int* n, blas_int* nrhs, double* a, blas_int* lda, double* b, blas_int* ldb, double* work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dgels)(      trans,           m,           n,           nrhs,         a,           lda,         b,           ldb,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_cgels)(char* trans, blas_int* m, blas_int* n, blas_int* nrhs, void*   a, blas_int* lda, void*   b, blas_int* ldb, void*   work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_cgels)(      trans,           m,           n,           nrhs,         a,           lda,         b,           ldb,         work,           lwork,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_zgels)(char* trans, blas_int* m, blas_int* n, blas_int* nrhs, void*   a, blas_int* lda, void*   b, blas_int* ldb, void*   work, blas_int* lwork, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_zgels)(      trans,           m,           n,           nrhs,         a,           lda,         b,           ldb,         work,           lwork,           info)  )

// solve a triangular system of linear equations
MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_strtrs)(char* uplo, char* trans, char* diag, blas_int* n, blas_int* nrhs, const float*  a, blas_int* lda, float*  b, blas_int* ldb, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_strtrs)(      uplo,       trans,       diag,           n,           nrhs,               a,           lda,         b,           ldb,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dtrtrs)(char* uplo, char* trans, char* diag, blas_int* n, blas_int* nrhs, const double* a, blas_int* lda, double* b, blas_int* ldb, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dtrtrs)(      uplo,       trans,       diag,           n,           nrhs,               a,           lda,         b,           ldb,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_ctrtrs)(char* uplo, char* trans, char* diag, blas_int* n, blas_int* nrhs, const void*   a, blas_int* lda, void*   b, blas_int* ldb, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_ctrtrs)(      uplo,       trans,       diag,           n,           nrhs,               a,           lda,         b,           ldb,           info)  )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_ztrtrs)(char* uplo, char* trans, char* diag, blas_int* n, blas_int* nrhs, const void*   a, blas_int* lda, void*   b, blas_int* ldb, blas_int* info)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_ztrtrs)(      uplo,       trans,       diag,           n,           nrhs,               a,           lda,         b,           ldb,           info)  )


// Armadillo BLAS functions

MADLIB_FORTRAN_QUALIFIER
float      arma_fortran(arma_sdot)(blas_int* n, const float*  x, blas_int* incx, const float*  y, blas_int* incy)
MADLIB_FORTRAN_DECLARE
( MADLIB_FORTRAN_RETURN(arma_sdot)(          n,               x,           incx,               y,           incy) )

MADLIB_FORTRAN_QUALIFIER
double     arma_fortran(arma_ddot)(blas_int* n, const double* x, blas_int* incx, const double* y, blas_int* incy)
MADLIB_FORTRAN_DECLARE
( MADLIB_FORTRAN_RETURN(arma_ddot)(          n,               x,           incx,               y,           incy) )


MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_sgemv)(const char* transA, const blas_int* m, const blas_int* n, const float*  alpha, const float*  A, const blas_int* ldA, const float*  x, const blas_int* incx, const float*  beta, float*  y, const blas_int* incy)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_sgemv)(            transA,                 m,                 n,               alpha,               A,                 ldA,               x,                 incx,               beta,         y,                 incy) )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dgemv)(const char* transA, const blas_int* m, const blas_int* n, const double* alpha, const double* A, const blas_int* ldA, const double* x, const blas_int* incx, const double* beta, double* y, const blas_int* incy)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dgemv)(            transA,                 m,                 n,               alpha,               A,                 ldA,               x,                 incx,               beta,         y,                 incy) )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_cgemv)(const char* transA, const blas_int* m, const blas_int* n, const void*   alpha, const void*   A, const blas_int* ldA, const void*   x, const blas_int* incx, const void*   beta, void*   y, const blas_int* incy)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_cgemv)(            transA,                 m,                 n,               alpha,               A,                 ldA,               x,                 incx,               beta,         y,                 incy) )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_zgemv)(const char* transA, const blas_int* m, const blas_int* n, const void*   alpha, const void*   A, const blas_int* ldA, const void*   x, const blas_int* incx, const void*   beta, void*   y, const blas_int* incy)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_zgemv)(            transA,                 m,                 n,               alpha,               A,                 ldA,               x,                 incx,               beta,         y,                 incy) )


MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_sgemm)(const char* transA, const char* transB, const blas_int* m, const blas_int* n, const blas_int* k, const float*  alpha, const float*  A, const blas_int* ldA, const float*  B, const blas_int* ldB, const float*  beta, float*  C, const blas_int* ldC)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_sgemm)(            transA,             transB,                 m,                 n,                 k,               alpha,               A,                 ldA,               B,                 ldB,               beta,         C,                 ldC) )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_dgemm)(const char* transA, const char* transB, const blas_int* m, const blas_int* n, const blas_int* k, const double* alpha, const double* A, const blas_int* ldA, const double* B, const blas_int* ldB, const double* beta, double* C, const blas_int* ldC)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_dgemm)(            transA,             transB,                 m,                 n,                 k,               alpha,               A,                 ldA,               B,                 ldB,               beta,         C,                 ldC) )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_cgemm)(const char* transA, const char* transB, const blas_int* m, const blas_int* n, const blas_int* k, const void*   alpha, const void*   A, const blas_int* ldA, const void*   B, const blas_int* ldB, const void*   beta, void*   C, const blas_int* ldC)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_cgemm)(            transA,             transB,                 m,                 n,                 k,               alpha,               A,                 ldA,               B,                 ldB,               beta,         C,                 ldC) )

MADLIB_FORTRAN_QUALIFIER
void arma_fortran(arma_zgemm)(const char* transA, const char* transB, const blas_int* m, const blas_int* n, const blas_int* k, const void*   alpha, const void*   A, const blas_int* ldA, const void*   B, const blas_int* ldB, const void*   beta, void*   C, const blas_int* ldC)
MADLIB_FORTRAN_DECLARE
(  MADLIB_FORTRAN(arma_zgemm)(            transA,             transB,                 m,                 n,                 k,               alpha,               A,                 ldA,               B,                 ldB,               beta,         C,                 ldC) )
