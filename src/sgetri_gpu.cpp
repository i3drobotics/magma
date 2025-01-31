/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from src/zgetri_gpu.cpp, normal z -> s, Sat Mar 27 20:30:34 2021

*/
#include "magma_internal.h"

// === Define what BLAS to use ============================================
#undef  magma_strsm
#define magma_strsm magmablas_strsm
// === End defining what BLAS to use ======================================

/***************************************************************************//**
    Purpose
    -------
    SGETRI computes the inverse of a matrix using the LU factorization
    computed by SGETRF. This method inverts U and then computes inv(A) by
    solving the system inv(A)*L = inv(U) for inv(A).
    
    Note that it is generally both faster and more accurate to use SGESV,
    or SGETRF and SGETRS, to solve the system AX = B, rather than inverting
    the matrix and multiplying to form X = inv(A)*B. Only in special
    instances should an explicit inverse be computed with this routine.

    Arguments
    ---------
    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in,out]
    dA      REAL array on the GPU, dimension (LDDA,N)
            On entry, the factors L and U from the factorization
            A = P*L*U as computed by SGETRF_GPU.
            On exit, if INFO = 0, the inverse of the original matrix A.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A.  LDDA >= max(1,N).

    @param[in]
    ipiv    INTEGER array, dimension (N)
            The pivot indices from SGETRF; for 1 <= i <= N, row i of the
            matrix was interchanged with row IPIV(i).

    @param[out]
    dwork   (workspace) REAL array on the GPU, dimension (MAX(1,LWORK))
  
    @param[in]
    lwork   INTEGER
            The dimension of the array DWORK.  LWORK >= N*NB, where NB is
            the optimal blocksize returned by magma_get_sgetri_nb(n).
    \n
            Unlike LAPACK, this version does not currently support a
            workspace query, because the workspace is on the GPU.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
      -     > 0:  if INFO = i, U(i,i) is exactly zero; the matrix is
                  singular and its cannot be computed.

    @ingroup magma_getri
*******************************************************************************/
extern "C" magma_int_t
magma_sgetri_gpu(
    magma_int_t n,
    magmaFloat_ptr dA, magma_int_t ldda, magma_int_t *ipiv,
    magmaFloat_ptr dwork, magma_int_t lwork,
    magma_int_t *info )
{
    #define dA(i, j)  (dA + (i) + (j)*ldda)
    #define dL(i, j)  (dL + (i) + (j)*lddl)
    
    /* Constants */
    const float c_zero    = MAGMA_S_ZERO;
    const float c_one     = MAGMA_S_ONE;
    const float c_neg_one = MAGMA_S_NEG_ONE;
    
    /* Local variables */
    magmaFloat_ptr dL;
    magma_int_t nb = magma_get_sgetri_nb( n );
    magma_int_t j, jmax, jb, jp, lddl;
    
    *info = 0;
    if (n < 0)
        *info = -1;
    else if (ldda < max(1,n))
        *info = -3;
    else if ( lwork < n*nb )
        *info = -6;

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    /* Quick return if possible */
    if ( n == 0 )
        return *info;
    
    if (lwork >= ldda*n) {
        lddl = ldda;
    }
    else {
        lddl = n;
    }
    dL = dwork;
    
    /* Invert the triangular factor U */
    magma_strtri_gpu( MagmaUpper, MagmaNonUnit, n, dA, ldda, info );
    if ( *info != 0 )
        return *info;
    
    magma_queue_t queue = NULL;
    magma_device_t cdev;
    magma_getdevice( &cdev );
    magma_queue_create( cdev, &queue );
    
    jmax = ((n-1) / nb)*nb;
    for( j = jmax; j >= 0; j -= nb ) {
        jb = min( nb, n-j );
        
        // copy current block column of A to work space dL
        // (only needs lower trapezoid, but we also copy upper triangle),
        // then zero the strictly lower trapezoid block column of A.
        magmablas_slacpy( MagmaFull, n-j, jb,
                          dA(j,j), ldda,
                          dL(j,0), lddl, queue );
        magmablas_slaset( MagmaLower, n-j-1, jb, c_zero, c_zero, dA(j+1,j), ldda, queue );
        
        // compute current block column of Ainv
        // Ainv(:, j:j+jb-1)
        //   = ( U(:, j:j+jb-1) - Ainv(:, j+jb:n) L(j+jb:n, j:j+jb-1) )
        //   * L(j:j+jb-1, j:j+jb-1)^{-1}
        // where L(:, j:j+jb-1) is stored in dL.
        if ( j+jb < n ) {
            magma_sgemm( MagmaNoTrans, MagmaNoTrans, n, jb, n-j-jb,
                         c_neg_one, dA(0,j+jb), ldda,
                                    dL(j+jb,0), lddl,
                         c_one,     dA(0,j),    ldda, queue );
        }
        // TODO use magmablas work interface
        magma_strsm( MagmaRight, MagmaLower, MagmaNoTrans, MagmaUnit,
                     n, jb, c_one,
                     dL(j,0), lddl,
                     dA(0,j), ldda, queue );
    }

    // Apply column interchanges
    for( j = n-2; j >= 0; --j ) {
        jp = ipiv[j] - 1;
        if ( jp != j ) {
            magmablas_sswap( n, dA(0,j), 1, dA(0,jp), 1, queue );
        }
    }
    
    magma_queue_destroy( queue );
    
    return *info;
}
