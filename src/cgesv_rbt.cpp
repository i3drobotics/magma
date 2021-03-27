/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from src/zgesv_rbt.cpp, normal z -> c, Sat Mar 27 20:30:35 2021

*/
#include "magma_internal.h"

/***************************************************************************//**
    Purpose
    -------
    CGESV_RBT solves a system of linear equations
        A * X = B
    where A is a general N-by-N matrix and X and B are N-by-NRHS matrices.
    Random Butterfly Tranformation is applied on A and B, then
    the LU decomposition with no pivoting is
    used to factor A as
        A = L * U,
    where L is unit lower triangular, and U is
    upper triangular.  The factored form of A is then used to solve the
    system of equations A * X = B.
    The solution can then be improved using iterative refinement.

    Arguments
    ---------
    @param[in]
    refine  magma_bool_t
            Specifies if iterative refinement is to be applied to improve the solution.
      -     = MagmaTrue:   Iterative refinement is applied.
      -     = MagmaFalse:  Iterative refinement is not applied.

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in]
    nrhs    INTEGER
            The number of right hand sides, i.e., the number of columns
            of the matrix B.  NRHS >= 0.

    @param[in,out]
    A       COMPLEX array, dimension (LDA,N).
            On entry, the M-by-N matrix to be factored.
            On exit, the factors L and U from the factorization
            A = P*L*U; the unit diagonal elements of L are not stored.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[in,out]
    B       COMPLEX array, dimension (LDB,NRHS)
            On entry, the right hand side matrix B.
            On exit, the solution matrix X.
    
    @param[in]
    ldb     INTEGER
            The leading dimension of the array B.  LDB >= max(1,N).

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value

    @ingroup magma_gesv_rbt
*******************************************************************************/
extern "C" magma_int_t
magma_cgesv_rbt(
    magma_bool_t refine, magma_int_t n, magma_int_t nrhs,
    magmaFloatComplex *A, magma_int_t lda,
    magmaFloatComplex *B, magma_int_t ldb,
    magma_int_t *info)
{
    /* Constants */
    const magmaFloatComplex c_zero = MAGMA_C_ZERO;
    const magmaFloatComplex c_one  = MAGMA_C_ONE;
    
    /* Local variables */
    magma_int_t nn = magma_roundup( n, 4 );  // n + ((4-(n % 4))%4);
    magmaFloatComplex *hu=NULL, *hv=NULL;
    magmaFloatComplex_ptr dA=NULL, dB=NULL, dAo=NULL, dBo=NULL, dwork=NULL, dv=NULL;
    magma_int_t i, iter;
    magma_queue_t queue=NULL;
    
    /* Function Body */
    *info = 0;
    if ( ! (refine == MagmaTrue) &&
         ! (refine == MagmaFalse) ) {
        *info = -1;
    }
    else if (n < 0) {
        *info = -2;
    } else if (nrhs < 0) {
        *info = -3;
    } else if (lda < max(1,n)) {
        *info = -5;
    } else if (ldb < max(1,n)) {
        *info = -7;
    }
    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if (nrhs == 0 || n == 0)
        return *info;

    if (MAGMA_SUCCESS != magma_cmalloc( &dA, nn*nn ) ||
        MAGMA_SUCCESS != magma_cmalloc( &dB, nn*nrhs ))
    {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        goto cleanup;
    }

    if (refine == MagmaTrue) {
        if (MAGMA_SUCCESS != magma_cmalloc( &dAo,   nn*nn ) ||
            MAGMA_SUCCESS != magma_cmalloc( &dwork, nn*nrhs ) ||
            MAGMA_SUCCESS != magma_cmalloc( &dBo,   nn*nrhs ))
        {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            goto cleanup;
        }
    }

    if (MAGMA_SUCCESS != magma_cmalloc_cpu( &hu, 2*nn ) ||
        MAGMA_SUCCESS != magma_cmalloc_cpu( &hv, 2*nn ))
    {
        *info = MAGMA_ERR_HOST_ALLOC;
        goto cleanup;
    }

    magma_device_t cdev;
    magma_getdevice( &cdev );
    magma_queue_create( cdev, &queue );
    
    magmablas_claset( MagmaFull, nn, nn, c_zero, c_one, dA, nn, queue );

    /* Send matrix to the GPU */
    magma_csetmatrix( n, n, A, lda, dA, nn, queue );

    /* Send b to the GPU */
    magma_csetmatrix( n, nrhs, B, ldb, dB, nn, queue );

    *info = magma_cgerbt_gpu( MagmaTrue, nn, nrhs, dA, nn, dB, nn, hu, hv, info );
    if (*info != MAGMA_SUCCESS)  {
        return *info;
    }

    if (refine == MagmaTrue) {
        magma_ccopymatrix( nn, nn, dA, nn, dAo, nn, queue );
        magma_ccopymatrix( nn, nrhs, dB, nn, dBo, nn, queue );
    }
    /* Solve the system U^TAV.y = U^T.b on the GPU */
    magma_cgesv_nopiv_gpu( nn, nrhs, dA, nn, dB, nn, info );

    /* Iterative refinement */
    if (refine == MagmaTrue) {
        magma_cgerfs_nopiv_gpu( MagmaNoTrans, nn, nrhs, dAo, nn, dBo, nn, dB, nn, dwork, dA, &iter, info );
    }
    //printf("iter = %lld\n", (long long) iter );

    /* The solution of A.x = b is Vy computed on the GPU */
    if (MAGMA_SUCCESS != magma_cmalloc( &dv, 2*nn )) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        goto cleanup;
    }

    magma_csetvector( 2*nn, hv, 1, dv, 1, queue );
    
    for (i = 0; i < nrhs; i++) {
        magmablas_cprbt_mv( nn, dv, dB+(i*nn), queue );
    }

    magma_cgetmatrix( n, nrhs, dB, nn, B, ldb, queue );

cleanup:
    magma_queue_destroy( queue );
    
    magma_free_cpu( hu );
    magma_free_cpu( hv );

    magma_free( dA );
    magma_free( dv );
    magma_free( dB );
    
    if (refine == MagmaTrue) {
        magma_free( dAo );
        magma_free( dBo );
        magma_free( dwork );
    }
    
    return *info;
}
