/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from src/zhetrf_nopiv_cpu.cpp, normal z -> d, Sat Mar 27 20:30:47 2021
 
*/
#include "magma_internal.h"

#define REAL

// TODO convert to usual (A + (i) + (j)*lda), i.e., returns pointer?
#define  A(i, j) ( A[(j)*lda  + (i)])
#define  C(i, j) ( C[(j)*ldc  + (i)])
#define  D(i)    ( D[(i)*incD] )


/******************************************************************************/
// TODO: change alpha and beta to be double, per BLAS, instead of double-real
// trailing submatrix update with inner-blocking
magma_int_t dsyrk_d(
    magma_uplo_t uplo, magma_int_t m, magma_int_t n,
    double alpha, double *A, magma_int_t lda,
    double beta,  double *C, magma_int_t ldc,
    double *D, magma_int_t incD)
{
    magma_int_t i, j, k;
    double *Aik;
    double *Dkk;
    double *Akj;

    /* Check input arguments */
    magma_int_t info = 0;
    if ((uplo != MagmaLower) && (uplo != MagmaUpper)) {
        info = -1;
    }
    if (m < 0) {
        info = -3;
    }
    if (n < 0) {
        info = -4;
    }
    if ((lda < max(1, m)) && (m > 0)) {
        info = -7;
    }
    if ((ldc < max(1, m)) && (m > 0)) {
        info = -10;
    }
    if ( incD < 0 ) {
        info = -12;
    }
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return info;
    }

    /* Quick return */
    if (m == 0 || n == 0 ||
        ((alpha == 0.0 || m == 0) && beta == 1.0) ) {
        return info;
    }

    if ( uplo == MagmaLower ) {
        for (j=0; j < m; j++) {
            for (i=j; i < m; i++) {
                double tmp = MAGMA_D_ZERO;
                Aik = A+i;
                Dkk = D;
                Akj = A+j;
                for (k=0; k < n; k++) {
                    tmp = tmp + (*Aik) * (*Dkk) * conj( *Akj );
                    Aik += lda;
                    Dkk += incD;
                    Akj += lda;
                }
                C(i, j) = beta * C(i, j) + alpha * tmp;
            }
        }
    }
    else {
        for (j=0; j < m; j++) {
            for (i=0; i <= j; i++) {
                double tmp = MAGMA_D_ZERO;
                for (k=0; k < n; k++) {
                    tmp = tmp + A(i, k) * D( k ) * conj( A(k, j) );
                }
                C(i, j) = beta * C(i, j) + alpha * tmp;
            }
        }
    }
    return info;
}


/******************************************************************************/
// TODO: change alpha and beta to be double, per BLAS, instead of double-real
// trailing submatrix update with inner-blocking, using workshpace that
// stores D*L'
magma_int_t dsyrk_d_workspace(
    magma_uplo_t uplo, magma_int_t n, magma_int_t k,
    double alpha, double *A, magma_int_t lda,
    double beta,  double *C, magma_int_t ldc,
    double *work, magma_int_t ldw)
{
    double c_one     = MAGMA_D_ONE;
    double c_neg_one = MAGMA_D_NEG_ONE;

    /* Check input arguments */
    magma_int_t info = 0;
    if ((uplo != MagmaLower) && (uplo != MagmaUpper)) {
        info = -1;
    }
    if (n < 0) {
        info = -2;
    }
    if (k < 0) {
        info = -3;
    }
    if ((lda < max(1,n)) && (n > 0)) {
        info = -6;
    }
    if ((ldc < max(1,n)) && (n > 0)) {
        info = -9;
    }
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return info;
    }

    /* Quick return */
    if (n == 0 || k == 0 ||
        ((alpha == 0.0 || k == 0) && beta == 1.0) ) {
        return info;
    }

    if ( uplo == MagmaLower ) {
        blasf77_dgemm( MagmaNoTransStr, MagmaNoTransStr,
                       &n, &n, &k,
                       &c_neg_one, A,    &lda,
                                   work, &ldw,
                       &c_one,     C,    &ldc );
    }
    else {
        blasf77_dgemm( MagmaNoTransStr, MagmaNoTransStr,
                       &n, &n, &k,
                       &c_neg_one, work, &ldw,
                                   A,    &lda,
                       &c_one,     C,    &ldc );
    }
    return info;
}


/******************************************************************************/
// diagonal factorization with inner-block
magma_int_t dsytrf_diag_nopiv(
    magma_uplo_t uplo, magma_int_t n,
    double *A, magma_int_t lda)
{
    const magma_int_t ione = 1;
    const double d_one = 1.0;
    
    /* Check input arguments */
    magma_int_t info = 0;
    if (lda < n) {
        info = -4;
    }
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return info;
    }

    /* Quick return */
    if (n == 1)
        return info;

    double *Ak1k = NULL;
    double *Akk = NULL;
    double alpha;

    if ( uplo == MagmaLower ) {
        /* Diagonal element */
        Akk  = A;

        /* Pointer on first extra diagonal element */
        Ak1k = A + 1;

        for (magma_int_t k=n-1; k > 0; k--) {
            alpha = MAGMA_D_REAL( *Akk );
            if ( fabs(alpha) < lapackf77_dlamch("Epsilon") ) {
                info = k;
                return info;
            }
            *Akk = MAGMA_D_MAKE(alpha, 0.0);

            // scale off-diagonals
            alpha = d_one / alpha;
            blasf77_dscal(&k, &alpha, Ak1k, &ione);

            // update remaining
            alpha = - MAGMA_D_REAL( *Akk );
            blasf77_dsyr(MagmaLowerStr, &k,
                         &alpha, Ak1k, &ione, Ak1k + lda, &lda);

            /* Move to next diagonal element */
            if (k > 1) { 
                Ak1k += lda;
                Akk = Ak1k;
                Ak1k++;
            }
        }
    } else {
        /* Diagonal element */
        Akk  = A;

        /* Pointer on first extra diagonal element */
        Ak1k = A + lda;

        for (magma_int_t k=n-1; k > 0; k--) {
            alpha = MAGMA_D_REAL( *Akk );
            if ( fabs(alpha) < lapackf77_dlamch("Epsilon") ) {
                info = k;
                return info;
            }
            *Akk = MAGMA_D_MAKE(alpha, 0.0);

            // scale off-diagonals
            alpha = d_one / alpha;
            blasf77_dscal(&k, &alpha, Ak1k, &lda);

            // update remaining
            alpha = - MAGMA_D_REAL( *Akk );

            #ifdef COMPLEX
            lapackf77_dlacgv(&k, Ak1k, &lda);
            #endif
            blasf77_dsyr(MagmaUpperStr, &k,
                         &alpha, Ak1k, &lda, Ak1k + 1, &lda);
            #ifdef COMPLEX
            lapackf77_dlacgv(&k, Ak1k, &lda);
            #endif

            /* Move to next diagonal element */
            if (k > 1) {
                Ak1k ++;
                Akk = Ak1k;
                Ak1k += lda;
            }
        }
    }
    return info;
}


/******************************************************************************/
// main routine
extern "C" magma_int_t
magma_dsytrf_nopiv_cpu(
    magma_uplo_t uplo, magma_int_t n, magma_int_t ib,
    double *A, magma_int_t lda,
    magma_int_t *info)
{
    magma_int_t ione = 1;
    double alpha;
    double d_one = 1.0;
    double c_one     = MAGMA_D_ONE;
    double c_neg_one = MAGMA_D_NEG_ONE;

    /* Check input arguments */
    *info = 0;
    if (lda < n) {
        *info = -1;
        return *info;
    }

    /* Quick return */
    if (n == 1) {
        return *info;
    }

    if ( uplo == MagmaLower ) {
        for (magma_int_t i = 0; i < n; i += ib) {
            magma_int_t sb = min(n-i, ib);

            /* Factorize the diagonal block */
            *info = dsytrf_diag_nopiv(uplo, sb, &A(i, i), lda);
            if (*info != 0) return *info;

            if ( i + sb < n ) {
                magma_int_t height = n - i - sb;

                /* Solve the lower panel ( L21*D11 )*/
                blasf77_dtrsm(
                    MagmaRightStr, MagmaLowerStr,
                    MagmaConjTransStr, MagmaUnitStr,
                    &height, &sb,
                    &c_one, &A(i, i),    &lda,
                            &A(i+sb, i), &lda);

                /* Scale the block to divide by D */
                for (magma_int_t k=0; k < sb; k++) {
                    #define DSYRK_D_WORKSPACE
                    #ifdef DSYRK_D_WORKSPACE
                    for (magma_int_t ii=i+sb; ii < n; ii++) {
                        A(i+k, ii) = MAGMA_D_CONJ( A(ii, i+k) );
                    }
                    #endif
                    alpha = d_one / MAGMA_D_REAL(A(i+k, i+k));
                    blasf77_dscal(&height, &alpha, &A(i+sb, i+k), &ione);
                    A(i+k, i+k) = MAGMA_D_MAKE(MAGMA_D_REAL(A(i+k, i+k)), 0.0);
                }

                /* Update the trailing submatrix A22 = A22 - A21 * D11 * A21' */
                #ifdef DSYRK_D_WORKSPACE
                dsyrk_d_workspace( MagmaLower, height, sb,
                                   c_neg_one, &A(i+sb, i),    lda,    // A21
                                   c_one,     &A(i+sb, i+sb), lda,    // A22
                                              &A(i, i+sb),    lda );  // workspace, I am writing on upper part :)
                #else
                dsyrk_d( MagmaLower, height, sb,
                         c_neg_one, &A(i+sb, i),    lda,      // A21
                         c_one,     &A(i+sb, i+sb), lda,      // A22
                                    &A(i, i),       lda+1 );  // D11
                #endif
            }
        }
    } else {
        for (magma_int_t i = 0; i < n; i += ib) {
            magma_int_t sb = min(n-i, ib);

            /* Factorize the diagonal block */
            *info = dsytrf_diag_nopiv(uplo, sb, &A(i, i), lda);
            if (*info != 0) return *info;

            if ( i + sb < n ) {
                magma_int_t height = n - i - sb;

                /* Solve the lower panel ( L21*D11 )*/
                blasf77_dtrsm(
                    MagmaLeftStr, MagmaUpperStr,
                    MagmaConjTransStr, MagmaUnitStr,
                    &sb, &height,
                    &c_one, &A(i, i),    &lda,
                            &A(i, i+sb), &lda);

                /* Scale the block to divide by D */
                for (magma_int_t k=0; k < sb; k++) {
                    #define DSYRK_D_WORKSPACE
                    #ifdef DSYRK_D_WORKSPACE
                    for (magma_int_t ii=i+sb; ii < n; ii++) {
                        A(ii, i+k) = MAGMA_D_CONJ( A(i+k, ii) );
                    }
                    #endif
                    alpha = d_one / MAGMA_D_REAL(A(i+k, i+k));
                    blasf77_dscal(&height, &alpha, &A(i+k, i+sb), &lda);
                    A(i+k, i+k) = MAGMA_D_MAKE(MAGMA_D_REAL(A(i+k, i+k)), 0.0);
                }

                /* Update the trailing submatrix A22 = A22 - A21 * D11 * A21' */
                #ifdef DSYRK_D_WORKSPACE
                dsyrk_d_workspace( MagmaUpper, height, sb,
                                   c_neg_one, &A(i, i+sb),    lda,    // A21
                                   c_one,     &A(i+sb, i+sb), lda,    // A22
                                              &A(i+sb, i),    lda );  // workspace, I am writing on upper part :)
                #else
                dsyrk_d( MagmaUpper, height, sb,
                         c_neg_one, &A(i, i+sb),    lda,      // A21
                         c_one,     &A(i+sb, i+sb), lda,      // A22
                                    &A(i, i),       lda+1 );  // D11
                #endif
            }
        }
    }

    return *info;
}
