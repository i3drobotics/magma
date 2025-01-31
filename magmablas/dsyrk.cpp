/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from magmablas/zherk.cpp, normal z -> d, Sat Mar 27 20:31:19 2021
       @author Azzam Haidar 
       @author Ahmad Abdelfattah
*/

#include "magma_internal.h"
#define PRECISION_d

/******************************************************************************/
extern "C"
void magmablas_dsyrk_internal(
    magma_uplo_t uplo, magma_trans_t trans, 
    magma_int_t n, magma_int_t k, magma_int_t nb,
    double alpha,
    magmaDouble_ptr dA, magma_int_t ldda, 
    magmaDouble_ptr dB, magma_int_t lddb, 
    double beta,
    magmaDouble_ptr       dC, magma_int_t lddc, 
    magma_int_t conjugate, magma_queue_t queue)
{
    #define dA(i_, j_) (dA + (i_) + (j_)*ldda)
    #define dB(i_, j_) (dB + (i_) + (j_)*lddb)
    #define dC(i_, j_) (dC + (i_) + (j_)*lddc)
    
    magma_trans_t transA;
    magma_trans_t transB;
    magmaDouble_ptr dAi, dBi;
    
    if (trans == MagmaNoTrans) {
        transA = MagmaNoTrans;
        transB = ( conjugate == 0 ) ? MagmaTrans : MagmaTrans;
    } else {
        transA = ( conjugate == 0 ) ? MagmaTrans : MagmaTrans;
        transB = MagmaNoTrans;
    }

    magma_int_t ib;
    for( magma_int_t i = 0; i < n; i += ib ) {
        ib     = min( nb, n-i );
        if(uplo == MagmaLower){
            dAi = (trans == MagmaNoTrans) ? dA(i,0) : dA(0,i);
            dBi = (trans == MagmaNoTrans) ? dB(i,0) : dB(0,i);
        }
        else{
            dAi = (trans == MagmaNoTrans) ? dA(0,0) : dA(0,0);
            dBi = (trans == MagmaNoTrans) ? dB(i,0) : dB(0,i);
        }

        magmaDouble_ptr dCi = (uplo  == MagmaLower  ) ? dC(i,i) : dC(0,i);
        magma_int_t im = (uplo == MagmaLower) ? n-i : min(i+nb, n);
        magma_int_t in = ib;
        magma_dgemm( transA, transB, im, in, k,
                     alpha, dAi, ldda,
                            dBi, lddb,
                     beta,  dCi, lddc, queue);
    }
}

/******************************************************************************/
#if defined(PRECISION_c) || defined(PRECISION_z)
extern "C"
void magmablas_dsyrk(
    magma_uplo_t uplo, magma_trans_t trans, 
    magma_int_t n, magma_int_t k, 
    double alpha,
    magmaDouble_ptr dA, magma_int_t ldda, 
    double beta,
    magmaDouble_ptr dC, magma_int_t lddc, 
    magma_queue_t queue)
{
    magma_int_t info = 0;
    if ( uplo != MagmaUpper && uplo != MagmaLower )
        info = -1;
    else if ( trans != MagmaNoTrans && trans != MagmaConjTrans )
        info = -2;
    else if ( n < 0 )
        info = -3;
    else if ( k < 0 )
        info = -4;
    else if ( trans == MagmaNoTrans ? ldda < n : ldda < k )
        info = -7;
    else if ( lddc < n )
        info = -10;

    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return;  //info;
    }

    if ( n <= 0 || k <= 0 )
        return;

    // TODO: tune nb?
    magma_int_t nb = 512; 
    magmablas_dsyrk_internal(
        uplo, trans, 
        n, k, nb,
        MAGMA_D_MAKE(alpha, 0.), dA, ldda, dA, ldda, 
        MAGMA_D_MAKE(beta, 0.),  dC, lddc, 1, queue);
}
#endif


/******************************************************************************/
extern "C"
void magmablas_dsyrk(
    magma_uplo_t uplo, magma_trans_t trans, 
    magma_int_t n, magma_int_t k, 
    double alpha,
    magmaDouble_ptr dA, magma_int_t ldda, 
    double beta,
    magmaDouble_ptr dC, magma_int_t lddc, 
    magma_queue_t queue)
{
    magma_int_t info = 0;
    if ( uplo != MagmaUpper && uplo != MagmaLower )
        info = -1;
    #if defined(PRECISION_c) || defined(PRECISION_z)
    else if ( trans != MagmaNoTrans && trans != MagmaTrans )
    #else
    else if ( trans != MagmaNoTrans && trans != MagmaTrans && trans != MagmaConjTrans )
    #endif
        info = -2;
    else if ( n < 0 )
        info = -3;
    else if ( k < 0 )
        info = -4;
    else if ( trans == MagmaNoTrans ? ldda < n : ldda < k )
        info = -7;
    else if ( lddc < n )
        info = -10;

    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return;  //info;
    }

    if ( n <= 0 || k <= 0 )
        return;

    // TODO: tune nb?
    magma_int_t nb = 512; 
    magmablas_dsyrk_internal(
        uplo, trans, 
        n, k, nb,
        alpha, dA, ldda, dA, ldda, 
        beta,  dC, lddc, 0, queue);
}
