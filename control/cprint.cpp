/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Mark Gates
       @generated from control/zprint.cpp, normal z -> c, Sat Mar 27 20:30:28 2021

*/
#include "magma_internal.h"

#define COMPLEX

/***************************************************************************//**
    Purpose
    -------
    magma_cprint prints a matrix that is located on the CPU host.
    The output is intended to be Matlab compatible, to be useful in debugging.

    Arguments
    ---------
    @param[in]
    m       INTEGER
            The number of rows of the matrix A.  M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix A.  N >= 0.

    @param[in]
    A       COMPLEX array, dimension (LDA,N), on the CPU host.
            The M-by-N matrix to be printed.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,M).

    @ingroup magma_print
*******************************************************************************/
extern "C"
void magma_cprint(
    magma_int_t m, magma_int_t n,
    const magmaFloatComplex *A, magma_int_t lda )
{
    #define A(i,j) (A + (i) + (j)*lda)
    
    magma_int_t info = 0;
    if ( m < 0 )
        info = -1;
    else if ( n < 0 )
        info = -2;
    else if ( lda < max(1,m) )
        info = -4;
    
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return;  //info;
    }
    
    magmaFloatComplex c_zero = MAGMA_C_ZERO;
    
    if ( m == 1 ) {
        printf( "[ " );
    }
    else {
        printf( "[\n" );
    }
    for( int i = 0; i < m; ++i ) {
        for( int j = 0; j < n; ++j ) {
            if ( MAGMA_C_EQUAL( *A(i,j), c_zero )) {
                #ifdef COMPLEX
                printf( "   0.              " );
                #else
                printf( "   0.    " );
                #endif
            }
            else {
                #ifdef COMPLEX
                printf( " %8.4f+%8.4fi", MAGMA_C_REAL( *A(i,j) ), MAGMA_C_IMAG( *A(i,j) ));
                #else
                printf( " %8.4f", MAGMA_C_REAL( *A(i,j) ));
                #endif
            }
        }
        if ( m > 1 ) {
            printf( "\n" );
        }
        else {
            printf( " " );
        }
    }
    printf( "];\n" );
}


/***************************************************************************//**
    Purpose
    -------
    magma_cprint_gpu prints a matrix that is located on the GPU device.
    Internally, it allocates CPU memory and copies the matrix to the CPU.
    The output is intended to be Matlab compatible, to be useful in debugging.

    Arguments
    ---------
    @param[in]
    m       INTEGER
            The number of rows of the matrix A.  M >= 0.

    @param[in]
    n       INTEGER
            The number of columns of the matrix A.  N >= 0.

    @param[in]
    dA      COMPLEX array, dimension (LDDA,N), on the GPU device.
            The M-by-N matrix to be printed.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A.  LDDA >= max(1,M).

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_print
*******************************************************************************/
extern "C"
void magma_cprint_gpu(
    magma_int_t m, magma_int_t n,
    magmaFloatComplex_const_ptr dA, magma_int_t ldda,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    if ( m < 0 )
        info = -1;
    else if ( n < 0 )
        info = -2;
    else if ( ldda < max(1,m) )
        info = -4;
    
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return;  //info;
    }
    
    magma_int_t lda = m;
    magmaFloatComplex* A;
    magma_cmalloc_cpu( &A, lda*n );

    magma_cgetmatrix( m, n, dA, ldda, A, lda, queue );
    
    magma_cprint( m, n, A, lda );
    
    magma_free_cpu( A );
}
