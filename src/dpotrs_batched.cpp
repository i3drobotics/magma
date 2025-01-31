
/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date
       
       @author Azzam Haidar

       @generated from src/zpotrs_batched.cpp, normal z -> d, Sat Mar 27 20:31:12 2021
*/
#include "magma_internal.h"
#include "batched_kernel_param.h"

/***************************************************************************//**
    Purpose
    -------
    DPOTRS solves a system of linear equations A*X = B with a symmetric
    positive definite matrix A using the Cholesky factorization
    A = U**H*U or A = L*L**H computed by DPOTRF.

    Arguments
    ---------
    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangle of A is stored;
      -     = MagmaLower:  Lower triangle of A is stored.

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in]
    nrhs    INTEGER
            The number of right hand sides, i.e., the number of columns
            of the matrix B.  NRHS >= 0.

    @param[in]
    dA_array    Array of pointers, dimension (batchCount).
             Each is a DOUBLE PRECISION array on the GPU, dimension (LDDA,N)
             The triangular factor U or L from the Cholesky factorization
             A = U**H*U or A = L*L**H, as computed by DPOTRF.

    @param[in]
    ldda    INTEGER
            The leading dimension of each array A.  LDDA >= max(1,N).

    @param[in,out]
    dB_array      Array of pointers, dimension (batchCount).
             Each is a DOUBLE PRECISION array on the GPU, dimension (LDDB,NRHS)
             On entry, each pointer is a right hand side matrix B.
             On exit, the corresponding solution matrix X.

    @param[in]
    lddb    INTEGER
            The leading dimension of each array B.  LDDB >= max(1,N).

    @param[in]
    batchCount  INTEGER
                The number of matrices to operate on.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.


    @ingroup magma_potrs_batched
*******************************************************************************/
extern "C" magma_int_t
magma_dpotrs_batched(
                  magma_uplo_t uplo, magma_int_t n, magma_int_t nrhs,
                  double **dA_array, magma_int_t ldda,
                  double **dB_array, magma_int_t lddb,
                  magma_int_t batchCount, magma_queue_t queue)
{
    double c_one = MAGMA_D_ONE;
    magma_int_t info = 0;
    if ( uplo != MagmaUpper && uplo != MagmaLower )
        info = -1;
    if ( n < 0 )
        info = -2;
    if ( nrhs < 0)
        info = -3;
    if ( ldda < max(1, n) )
        info = -5;
    if ( lddb < max(1, n) )
        info = -7;
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return info;
    }

    /* Quick return if possible */
    if ( (n == 0) || (nrhs == 0) ) {
        return info;
    }
    
    double **dwork_array = NULL;
    double* dwork        = NULL; //dwork is workspace for dtrsv 
    
    if(nrhs == 1){
        magma_int_t dwork_msize = n*nrhs;        // TODO: resize dwork for trsv purpose only
        magma_malloc((void**)&dwork_array, batchCount * sizeof(*dwork_array));
        magma_dmalloc( &dwork, dwork_msize * batchCount );
        /* check allocation */
        if ( dwork_array == NULL || dwork     == NULL ) {
            magma_free(dwork_array);
            magma_free( dwork );
            info = MAGMA_ERR_DEVICE_ALLOC;
            magma_xerbla( __func__, -(info) );
            return info;
        }
        magmablas_dlaset( MagmaFull, dwork_msize, batchCount, MAGMA_D_ZERO, MAGMA_D_ZERO, dwork, dwork_msize, queue );
        magma_dset_pointer( dwork_array, dwork, n, 0, 0, dwork_msize, batchCount, queue );
    }

    if ( uplo == MagmaUpper) {
        if (nrhs > 1){
            // A = U^T U
            // solve U^{T} Y = B, where Y = U X 
            magmablas_dtrsm_batched(
                    MagmaLeft, MagmaUpper, MagmaConjTrans, MagmaNonUnit, 
                    n, nrhs, c_one, 
                    dA_array, ldda, 
                    dB_array, lddb, batchCount, queue );

            // solve U X = B
            magmablas_dtrsm_batched(
                    MagmaLeft, MagmaUpper, MagmaNoTrans, MagmaNonUnit, 
                    n, nrhs, c_one, 
                    dA_array, ldda, 
                    dB_array, lddb, batchCount, queue );
        }
        else{
            // A = U^T U
            // solve U^{T}X = B ==> dworkX = U^-T * B
            magmablas_dtrsv_outofplace_batched( MagmaUpper, MagmaConjTrans, MagmaNonUnit,
                    n, 
                    dA_array,       ldda, // dA
                    dB_array,      1, // dB
                    dwork_array,     // dX //output
                    batchCount, queue, 0 );

            // solve U X = dwork ==> X = U^-1 * dwork
            magmablas_dtrsv_outofplace_batched( MagmaUpper, MagmaNoTrans, MagmaNonUnit, 
                    n, 
                    dA_array,       ldda, // dA
                    dwork_array,        1, // dB 
                    dB_array,   // dX //output
                    batchCount, queue, 0 );
        }
    }
    else {
        if (nrhs > 1){
            // A = L L^T
            // solve LY=B, where Y = L^{T} X
            magmablas_dtrsm_batched(
                    MagmaLeft, MagmaLower, MagmaNoTrans, MagmaNonUnit, 
                    n, nrhs, 
                    c_one, 
                    dA_array, ldda, 
                    dB_array, lddb, batchCount, queue );

            // solve L^{T}X=B
            magmablas_dtrsm_batched(
                    MagmaLeft, MagmaLower, MagmaConjTrans, MagmaNonUnit, 
                    n, nrhs, c_one, 
                    dA_array, ldda, 
                    dB_array, lddb, batchCount, queue );
        }
        else
        {
            // A = L L^T
            // solve LX= B ==> dwork = L^{-1} B
            magmablas_dtrsv_outofplace_batched( MagmaLower, MagmaNoTrans, MagmaNonUnit, 
                    n,
                    dA_array,       ldda, // dA
                    dB_array,      1, // dB
                    dwork_array,   // dX //output
                    batchCount, queue, 0 );

            // solve L^{T}X= dwork ==> X = L^{-T} dwork
            magmablas_dtrsv_outofplace_batched( MagmaLower, MagmaConjTrans, MagmaNonUnit,
                    n,
                    dA_array,       ldda, // dA
                    dwork_array,        1, // dB 
                    dB_array,     // dX //output
                    batchCount, queue, 0 );
        }
    }

    magma_queue_sync(queue);

    if(nrhs == 1){
        magma_free(dwork_array);
        magma_free( dwork );
    }

    return info;
}
