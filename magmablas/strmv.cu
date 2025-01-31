/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from magmablas/ztrmv.cu, normal z -> s, Sat Mar 27 20:31:28 2021

       @author Ahmad Abdelfattah

*/
#include "magma_internal.h"
#include "batched_kernel_param.h"

#define PRECISION_s
#include "trmv_template_kernel.cuh"

magma_int_t magma_get_strmv_nb(magma_int_t n)
{
    if      ( n > 2048 ) return 2048;
    else if ( n > 1024 ) return 1024;
    else if ( n >  512 ) return 512;
    else if ( n >  256 ) return 256;
    else if ( n >  128 ) return 128;
    else if ( n >   64 ) return  64;
    else if ( n >   32 ) return  32;
    else if ( n >   16 ) return  16;
    else if ( n >    8 ) return   8;
    else if ( n >    4 ) return   4;
    else if ( n >    2 ) return   2;
    else return 1;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void
magmablas_strmv_small(
        magma_uplo_t uplo, magma_trans_t transA, magma_diag_t diag,
        magma_int_t n,
        float *dA, magma_int_t ldda,
        float *dX, magma_int_t incx,
        magma_queue_t queue )
{
    if(transA == MagmaConjTrans) {
        trmv_template<float, STRMV_NB, 1>
        (uplo, transA, diag, n, dA, ldda, dX, incx, queue);
    }
    else {
        trmv_template<float, STRMV_NB, 0>
        (uplo, transA, diag, n, dA, ldda, dX, incx, queue);
    }
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void
magmablas_strmv_core(
        magma_uplo_t uplo, magma_trans_t transA, magma_diag_t diag,
        magma_int_t n,
        float *dA, magma_int_t ldda,
        float *dX, magma_int_t incx,
        magma_queue_t queue )
{
#define dA(i,j) dA + j*ldda + i
#define dX(j)   dX + j*incx

    const float c_one = MAGMA_S_ONE;

    // stopping condition
    if(n <= STRMV_NB){
        magmablas_strmv_small( uplo, transA, diag, n, dA, ldda, dX, incx, queue );
        return;
    }

    magma_int_t shape = 0;
    if      (transA == MagmaNoTrans  && uplo == MagmaLower) { shape = 0; } // NL
    else if (transA == MagmaNoTrans  && uplo == MagmaUpper) { shape = 1; } // NU
    else if (transA != MagmaNoTrans  && uplo == MagmaLower) { shape = 2; } // TL | CL
    else if (transA != MagmaNoTrans  && uplo == MagmaUpper) { shape = 3; } // TU | CU

    // at this point we can tell that nrowA > STRMV_NB
    switch(shape)
    {
        case 0: // Nl
            {
                const int n1 = magma_get_strmv_nb(n);
                const int n2 = n - n1;

                magmablas_strmv_core(
                        uplo, transA, diag, n2,
                        dA(n1, n1), ldda, dX(n1), incx, queue );

                magmablas_sgemv(
                        MagmaNoTrans, n2, n1,
                        c_one, dA(n1, 0), ldda,
                               dX( 0), incx,
                        c_one, dX(n1), incx,
                        queue );

                magmablas_strmv_core(
                        uplo, transA, diag, n1,
                        dA(0, 0), ldda, dX(0), incx, queue );
            }
            break;
        case 1: // NU
            {
                const int n2 = magma_get_strmv_nb(n);
                const int n1 = n - n2;

                magmablas_strmv_core(
                        uplo, transA, diag, n1,
                        dA(0, 0), ldda, dX(0), incx, queue );

                magma_sgemv(
                        MagmaNoTrans,
                        n1, n2,
                        c_one, dA( 0, n1), ldda,
                               dX(n1), incx,
                        c_one, dX( 0), incx,
                        queue );

                magmablas_strmv_core(
                        uplo, transA, diag, n2,
                        dA(n1, n1), ldda, dX(n1), incx, queue );
            }
            break;
        case 2: // TL || CL
            {
                const int n2 = magma_get_strmv_nb(n);
                const int n1 = n - n2;

                magmablas_strmv_core(
                        uplo, transA, diag, n1,
                        dA(0, 0), ldda, dX(0), incx, queue );

                magma_sgemv(
                        transA,
                        n2, n1,
                        c_one, dA(n1, 0), ldda,
                               dX(n1), incx,
                        c_one, dX( 0), incx,
                        queue );

                magmablas_strmv_core(
                        uplo, transA, diag, n2,
                        dA(n1, n1), ldda, dX(n1), incx, queue );
            }
            break;
        case 3: // TU | CU
            {
                const int n1 = magma_get_strmv_nb(n);
                const int n2 = n - n1;

                magmablas_strmv_core(
                        uplo, transA, diag, n2,
                        dA(n1, n1), ldda, dX(n1), incx, queue );

                magma_sgemv(
                        transA,
                        n1, n2,
                        c_one, dA(0, n1), ldda,
                               dX( 0), incx,
                        c_one, dX(n1), incx,
                        queue );

                magmablas_strmv_core(
                        uplo, transA, diag, n1,
                        dA(0, 0), ldda, dX(0), incx, queue );
            }
            break;
        default:; // propose something
    }
#undef dA
#undef dX
}

///////////////////////////////////////////////////////////////////////////////////////////////////
/*
    Purpose
    =======

    STRMV  performs the matrix-vector operation

       X := op( A )*X

    where A is a unit, or non-unit,  upper or lower triangular n-by-n matrix
    and  op( A )  is one  of

       op( A ) = A   or   op( A ) = A'   or   op( A ) = conjg( A' ).

    Parameters
    ==========
    @param[in]
    uplo    Whether the upper or lower triangle of A is referenced.

    @param[in]
    trans   Operation to perform on A.

    @param[in]
    diag    Whether the diagonal of A is assumed to be unit or non-unit.

    @param[in]
    n       Number of rows and columns of A. n >= 0.

    @param[in]
    dA      REAL array of dimension (ldda,n), ldda >= max(1,n).
            The n-by-n matrix A, on GPU device.
            Before entry with  UPLO = 'U' or 'u', the leading n-by-n upper
            triangular part of the array A must contain the upper triangular matrix
            and the strictly lower triangular part of A is not referenced.
            Before entry with UPLO = 'L' or 'l', the leading n-by-n lower triangular
            part of the array A must contain the lower triangular matrix and the
            strictly upper triangular part of A is not referenced.
            Note that when  DIAG = 'U' or 'u', the diagonal elements of A are not
            referenced either, but are assumed to be unity.
            Unchanged on exit.

    @param[in]
    ldda    Leading dimension of dA, must be at least max( 1, n ).

    @param[in,out]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).
            Before entry, the incremented array X must contain the n element
            vector x.
            On exit, X is overwritten with the transformed vector x.

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_trmv
*******************************************************************************/
extern "C" void
magmablas_strmv(
        magma_uplo_t uplo, magma_trans_t transA, magma_diag_t diag,
        magma_int_t n,
        float *dA, magma_int_t ldda,
        float *dX, magma_int_t incx,
        magma_queue_t queue )
{
    magma_int_t info = 0;
    if ( uplo != MagmaUpper && uplo != MagmaLower ) {
        info = -1;
    } else if ( transA != MagmaNoTrans && transA != MagmaTrans && transA != MagmaConjTrans ) {
        info = -2;
    } else if ( diag != MagmaUnit && diag != MagmaNonUnit ) {
        info = -3;
    } else if (n < 0) {
        info = -4;
    } else if (ldda < max(1,n)) {
        info = -6;
    } else if (incx < 0) {    // no support yet for a negative incx
        info = -8;
    }

    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return;
    }

    if ( n <= 0 )
        return;

    magmablas_strmv_core( uplo, transA, diag, n, dA, ldda, dX, incx, queue );

}
///////////////////////////////////////////////////////////////////////////////////////////////////
