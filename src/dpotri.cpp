/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from src/zpotri.cpp, normal z -> d, Sat Mar 27 20:30:31 2021

*/
#include "magma_internal.h"

/***************************************************************************//**
    Purpose
    -------
    DPOTRI computes the inverse of a real symmetric positive definite
    matrix A using the Cholesky factorization A = U**T*U or A = L*L**T
    computed by DPOTRF.

    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of A is stored;
      -     = MagmaLower:  Lower triangle of A is stored.

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in,out]
    A       DOUBLE PRECISION array, dimension (LDA,N)
            On entry, the triangular factor U or L from the Cholesky
            factorization A = U**T*U or A = L*L**T, as computed by
            DPOTRF.
            On exit, the upper or lower triangle of the (symmetric)
            inverse of A, overwriting the input factor U or L.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
      -     > 0:  if INFO = i, the (i,i) element of the factor U or L is
                  zero, and the inverse could not be computed.

    @ingroup magma_potri
*******************************************************************************/
extern "C" magma_int_t
magma_dpotri(
    magma_uplo_t uplo, magma_int_t n,
    double *A, magma_int_t lda,
    magma_int_t *info)
{
    /* Local variables */
    *info = 0;
    if ((uplo != MagmaUpper) && (uplo != MagmaLower))
        *info = -1;
    else if (n < 0)
        *info = -2;
    else if (lda < max(1,n))
        *info = -4;

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if ( n == 0 )
        return *info;
    
    /* Invert the triangular Cholesky factor U or L */
    magma_dtrtri( uplo, MagmaNonUnit, n, A, lda, info );
    if ( *info == 0 ) {
        /* Form inv(U) * inv(U)**T or inv(L)**T * inv(L) */
        magma_dlauum( uplo, n, A, lda, info );
    }
    
    return *info;
} /* magma_dpotri */
