/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from magmablas/zher2k_vbatched.cpp, normal z -> s, Sat Mar 27 20:31:43 2021

       @author Jakub Kurzak
       @author Stan Tomov
       @author Mark Gates
       @author Azzam Haidar
       @author Ahmad Abdelfattah

*/
#include "magma_internal.h"
#include "commonblas_s.h"

#define REAL

/******************************************************************************/
extern "C" void
magmablas_ssyr2k_vbatched_max_nocheck(
    magma_uplo_t uplo, magma_trans_t trans, magma_int_t* n, magma_int_t* k,
    float alpha,
    float const * const * dA_array, magma_int_t* ldda,
    float const * const * dB_array, magma_int_t* lddb, 
    float beta, float **dC_array, magma_int_t* lddc, 
    magma_int_t batchCount, 
    magma_int_t max_n, magma_int_t max_k, magma_queue_t queue )
{
    float cbeta = MAGMA_S_MAKE(beta, 0.);
    float c_one = MAGMA_S_MAKE(1., 0.);
        
    if( trans == MagmaNoTrans){
        magmablas_ssyrk_internal_vbatched(uplo, MagmaNoTrans, n, k, alpha, dA_array, ldda, dB_array, lddb, cbeta, dC_array, lddc, max_n, max_k, batchCount, queue );
        magmablas_ssyrk_internal_vbatched(uplo, MagmaNoTrans, n, k, MAGMA_S_CONJ(alpha), dB_array, lddb, dA_array, ldda, c_one, dC_array, lddc, max_n, max_k, batchCount, queue );    
    }else{
        magmablas_ssyrk_internal_vbatched(uplo, MagmaTrans, n, k, alpha, dA_array, ldda, dB_array, lddb, cbeta, dC_array, lddc, max_n, max_k, batchCount, queue );
        magmablas_ssyrk_internal_vbatched(uplo, MagmaTrans, n, k, MAGMA_S_CONJ(alpha), dB_array, lddb, dA_array, ldda, c_one, dC_array, lddc, max_n, max_k, batchCount, queue );
    }
}


/******************************************************************************/
extern "C" void
magmablas_ssyr2k_vbatched_max(
    magma_uplo_t uplo, magma_trans_t trans, magma_int_t* n, magma_int_t* k,
    float alpha,
    float const * const * dA_array, magma_int_t* ldda,
    float const * const * dB_array, magma_int_t* lddb, 
    float beta, float **dC_array, magma_int_t* lddc, 
    magma_int_t batchCount, 
    magma_int_t max_n, magma_int_t max_k, magma_queue_t queue )
{
    magma_int_t info = 0;
    #ifdef COMPLEX
    info =  magma_her2k_vbatched_checker(    uplo, trans, n, k, ldda, lddb, lddc, batchCount, queue );
    #else
    info =  magma_syr2k_vbatched_checker( 0, uplo, trans, n, k, ldda, lddb, lddc, batchCount, queue );
    #endif
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return;
    }
    
    magmablas_ssyr2k_vbatched_max_nocheck( 
            uplo, trans, 
            n, k, 
            alpha, dA_array, ldda, 
                   dB_array, lddb, 
            beta,  dC_array, lddc, 
            batchCount, max_n, max_k, queue );
}


/******************************************************************************/
extern "C" void
magmablas_ssyr2k_vbatched_nocheck(
    magma_uplo_t uplo, magma_trans_t trans, magma_int_t* n, magma_int_t* k,
    float alpha,
    float const * const * dA_array, magma_int_t* ldda,
    float const * const * dB_array, magma_int_t* lddb, 
    float beta, float **dC_array, magma_int_t* lddc, 
    magma_int_t batchCount, magma_queue_t queue )
{
    // compute the max. dimensions
    magma_imax_size_2(n, k, batchCount, queue);
    magma_int_t max_n, max_k; 
    magma_igetvector_async(1, &n[batchCount], 1, &max_n, 1, queue);
    magma_igetvector_async(1, &k[batchCount], 1, &max_k, 1, queue);
    magma_queue_sync( queue );

    magmablas_ssyr2k_vbatched_max_nocheck( 
            uplo, trans, 
            n, k, 
            alpha, dA_array, ldda, 
                   dB_array, lddb, 
            beta,  dC_array, lddc, 
            batchCount, max_n, max_k, queue );
}


/***************************************************************************//**
    Purpose
    -------
    SSYR2K  performs one of the symmetric rank 2k operations
   
        C := alpha*A*B**H + conjg( alpha )*B*A**H + beta*C,
   
    or
   
        C := alpha*A**H*B + conjg( alpha )*B**H*A + beta*C,
   
    where  alpha and beta  are scalars with  beta  real,  C is an  n by n
    symmetric matrix and  A and B  are  n by k matrices in the first case
    and  k by n  matrices in the second case.
    
    Parameters
    ----------
    @param[in]
    uplo     magma_uplo_t.
             On entry, UPLO specifies whether the upper or lower
             triangular part of the array C is to be referenced as
             follows:
      -     = MagmaUpper:  Only the upper triangular part of C is to be referenced.
      -     = MagmaLower:  Only the lower triangular part of C is to be referenced.

    @param[in]
    trans    magma_trans_t.
             On entry, TRANS specifies the operation to be performed as
             follows:
      -     = MagmaNoTrans:     C := alpha*A*B**H + conj( alpha )*B*A**H + beta*C.
      -     = MagmaTrans:  C := alpha*A**H*B + conj( alpha )*B**H*A + beta*C.

    @param[in]
    n        Array of integers, size(batchCount + 1).
             On entry, each INTEGER N specifies the order of the corresponding matrix C. 
             N must be at least zero. 
             The last element of the array is used internally by the routine. 

    @param[in]
    k        Array of integers, size(batchCount + 1).
             On entry with TRANS = MagmaNoTrans, each INTEGER K specifies the number
             of columns of the corresponding matrices A and B, and on entry with
             TRANS = MagmaTrans, K specifies the number of rows of the
             corresponding matrices A and B. K must be at least zero.
             The last element of the array is used internally by the routine. 

    @param[in]
    alpha    REAL.
             On entry, ALPHA specifies the scalar alpha.
    
    @param[in]
    dA_array      Array of pointers, dimension (batchCount).
             Each is a REAL array of DIMENSION ( LDDA, Ka ), where Ka is
             K when TRANS = MagmaNoTrans, and is N otherwise.
             Before entry with TRANS = MagmaNoTrans, the leading N by K
             part of the array A must contain the matrix A, otherwise
             the leading K by N part of the array A must contain the
             matrix A.
    
    @param[in]
    ldda    Array of integers, size(batchCount + 1).
            On entry, each INTEGER LDDA specifies the first dimension of the 
            corresponding matrix A as declared in the calling (sub) program. 
            When TRANS = MagmaNoTrans then LDDA must be at least max( 1, N ), 
            otherwise LDDA must be at least max( 1, K ). 
            The last element of the array is used internally by the routine. 
    
    @param[in]
    dB_array      Array of pointers, dimension (batchCount).
             Each is a REAL array of DIMENSION ( ldb, kb ), where kb is
             k  when  TRANS = MagmaNoTrans,  and is  n  otherwise.
             Before entry with  TRANS = MagmaNoTrans,  the  leading  n by k
             part of the array  B  must contain the matrix  B,  otherwise
             the leading  k by n  part of the array  B  must contain  the
             matrix B.
    
    @param[in]
    lddb     Array of integers, size(batchCount + 1).
             On entry, each INTEGER LDDB specifies the first dimension of the 
             corresponding matrix B as declared in  the  calling  (sub)  program.   
             When  TRANS = MagmaNoTrans then  LDDB must be at least  max( 1, N ), 
             otherwise  LDDB must be at least  max( 1, K ). Unchanged on exit.
             The last element of the array is used internally by the routine. 
    
    @param[in]
    beta    REAL.
            On entry,  BETA  specifies the scalar  beta.  
    
    @param[in,out]
    dC_array      Array of pointers, dimension (batchCount).
             Each is REAL array of DIMENSION ( lddc, n ).
             Before entry  with  UPLO = MagmaUpper,  the leading  n by n
             upper triangular part of the array C must contain the upper
             triangular part  of the  symmetric matrix  and the strictly
             lower triangular part of C is not referenced.  On exit, the
             upper triangular part of the array  C is overwritten by the
             upper triangular part of the updated matrix.
             Before entry  with  UPLO = MagmaLower,  the leading  n by n
             lower triangular part of the array C must contain the lower
             triangular part  of the  symmetric matrix  and the strictly
             upper triangular part of C is not referenced.  On exit, the
             lower triangular part of the array  C is overwritten by the
             lower triangular part of the updated matrix.
             Note that the imaginary parts of the diagonal elements need
             not be set,  they are assumed to be zero,  and on exit they
             are set to zero.

    @param[in]
    lddc    Array of integers, size(batchCount + 1).
            On entry, each INTEGER LDDC specifies the first dimension of the corresponding 
            matrix C as declared in  the  calling  (sub)  program. LDDC  must  be  at  least
            max( 1, N ).
            The last element of the array is used internally by the routine. 
    
    @param[in]
    batchCount  INTEGER
                The number of matrices to operate on.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_her2k_batched
*******************************************************************************/
extern "C" void
magmablas_ssyr2k_vbatched(
    magma_uplo_t uplo, magma_trans_t trans, magma_int_t* n, magma_int_t* k,
    float alpha,
    float const * const * dA_array, magma_int_t* ldda,
    float const * const * dB_array, magma_int_t* lddb, 
    float beta, float **dC_array, magma_int_t* lddc, 
    magma_int_t batchCount, magma_queue_t queue )
{
    magma_int_t info = 0;
    #ifdef COMPLEX
    info =  magma_her2k_vbatched_checker(    uplo, trans, n, k, ldda, lddb, lddc, batchCount, queue );
    #else
    info =  magma_syr2k_vbatched_checker( 0, uplo, trans, n, k, ldda, lddb, lddc, batchCount, queue );
    #endif
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return;
    }
    
    // compute the max. dimensions
    magma_imax_size_2(n, k, batchCount, queue);
    magma_int_t max_n, max_k; 
    magma_igetvector_async(1, &n[batchCount], 1, &max_n, 1, queue);
    magma_igetvector_async(1, &k[batchCount], 1, &max_k, 1, queue);
    magma_queue_sync( queue );

    magmablas_ssyr2k_vbatched_max_nocheck( 
            uplo, trans, 
            n, k, 
            alpha, dA_array, ldda, 
                   dB_array, lddb, 
            beta,  dC_array, lddc, 
            batchCount, max_n, max_k, queue );
}
