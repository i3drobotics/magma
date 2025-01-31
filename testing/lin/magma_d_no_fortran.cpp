/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Mark Gates
       @generated from testing/lin/magma_z_no_fortran.cpp, normal z -> d, Sat Mar 27 20:31:46 2021
       
       This is simply a copy of part of magma_dlapack.h,
       with the { printf(...); } function body added to each function.
*/
#include <stdio.h>

#include "magma_v2.h"
#include "magma_lapack.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REAL

static const char* format = "Cannot check results: %s unavailable, since there was no Fortran compiler.\n";

/*
 * Testing functions
 */
void   lapackf77_dbdt01( const magma_int_t *m, const magma_int_t *n, const magma_int_t *kd,
                         double *A, const magma_int_t *lda,
                         double *Q, const magma_int_t *ldq,
                         double *d, double *e,
                         double *Pt, const magma_int_t *ldpt,
                         double *work,
                         #ifdef COMPLEX
                         double *rwork,
                         #endif
                         double *resid )
                         { printf( format, __func__ ); }

void   lapackf77_dget22( const char *transa, const char *transe, const char *transw, const magma_int_t *n,
                         double *A, const magma_int_t *lda,
                         double *E, const magma_int_t *lde,
                         #ifdef COMPLEX
                         double *w,
                         #else
                         double *wr,
                         double *wi,
                         #endif
                         double *work,
                         #ifdef COMPLEX
                         double *rwork,
                         #endif
                         double *result )
                         { printf( format, __func__ ); }

void   lapackf77_dsyt21( const magma_int_t *itype, const char *uplo,
                         const magma_int_t *n, const magma_int_t *kband,
                         double *A, const magma_int_t *lda,
                         double *d, double *e,
                         double *U, const magma_int_t *ldu,
                         double *V, const magma_int_t *ldv,
                         double *tau,
                         double *work,
                         #ifdef COMPLEX
                         double *rwork,
                         #endif
                         double *result )
                         { printf( format, __func__ ); }

void   lapackf77_dsyt22( const magma_int_t *itype, const char *uplo,
                         const magma_int_t *n,  const magma_int_t *m, const magma_int_t *kband,
                         double *A, const magma_int_t *lda,
                         double *d, double *e,
                         double *U, const magma_int_t *ldu,
                         double *V, const magma_int_t *ldv,
                         double *tau,
                         double *work,
                         #ifdef COMPLEX
                         double *rwork,
                         #endif
                         double *result )
                         { printf( format, __func__ ); }

void   lapackf77_dhst01( const magma_int_t *n, const magma_int_t *ilo, const magma_int_t *ihi,
                         double *A, const magma_int_t *lda,
                         double *H, const magma_int_t *ldh,
                         double *Q, const magma_int_t *ldq,
                         double *work, const magma_int_t *lwork,
                         #ifdef COMPLEX
                         double *rwork,
                         #endif
                         double *result )
                         { printf( format, __func__ ); }

void   lapackf77_dstt21( const magma_int_t *n, const magma_int_t *kband,
                         double *AD,
                         double *AE,
                         double *SD,
                         double *SE,
                         double *U, const magma_int_t *ldu,
                         double *work,
                         #ifdef COMPLEX
                         double *rwork,
                         #endif
                         double *result )
                         { printf( format, __func__ ); }

void   lapackf77_dort01( const char *rowcol, const magma_int_t *m, const magma_int_t *n,
                         double *U, const magma_int_t *ldu,
                         double *work, const magma_int_t *lwork,
                         #ifdef COMPLEX
                         double *rwork,
                         #endif
                         double *resid )
                         { printf( format, __func__ ); }

// testing/eig
void   lapackf77_dlarfy( const char *uplo, const magma_int_t *n,
                         double *V, const magma_int_t *incv,
                         double *tau,
                         double *C, const magma_int_t *ldc,
                         double *work )
                         { printf( format, __func__ ); }

double lapackf77_dqpt01( const magma_int_t *m, const magma_int_t *n, const magma_int_t *k,
                         double *A,
                         double *Af, const magma_int_t *lda,
                         double *tau, magma_int_t *jpvt,
                         double *work, const magma_int_t *lwork )
                         { printf( format, __func__ ); return -1; }

void   lapackf77_dqrt02( const magma_int_t *m, const magma_int_t *n, const magma_int_t *k,
                         double *A,
                         double *AF,
                         double *Q,
                         double *R, const magma_int_t *lda,
                         double *tau,
                         double *work, const magma_int_t *lwork,
                         double *rwork,
                         double *result )
                         { printf( format, __func__ ); }

// testing/matgen
void   lapackf77_dlatms( const magma_int_t *m, const magma_int_t *n,
                         const char *dist, magma_int_t *iseed, const char *sym,
                         double *d,
                         const magma_int_t *mode, const double *cond,
                         const double *dmax,
                         const magma_int_t *kl, const magma_int_t *ku,
                         const char *pack,
                         double *A, const magma_int_t *lda,
                         double *work,
                         magma_int_t *info )
                         { printf( format, __func__ ); }

#ifdef __cplusplus
}
#endif
