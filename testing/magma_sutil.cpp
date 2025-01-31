/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/magma_zutil.cpp, normal z -> s, Sat Mar 27 20:31:46 2021

       @author Mark Gates

       Utilities for testing.
*/

#include <algorithm>  // sort

#include "magma_v2.h"
#include "magma_lapack.h"
#include "../control/magma_threadsetting.h"  // internal header, to work around MKL bug

#include "testings.h"

#define REAL

#define A(i,j)  A[i + j*lda]

// --------------------
// Make a matrix symmetric/symmetric.
// Makes diagonal real.
// Sets Aji = conj( Aij ) for j < i, that is, copy & conjugate lower triangle to upper triangle.
extern "C"
void magma_smake_symmetric( magma_int_t N, float* A, magma_int_t lda )
{
    magma_int_t i, j;
    for( i=0; i < N; ++i ) {
        A(i,i) = MAGMA_S_MAKE( MAGMA_S_REAL( A(i,i) ), 0. );
        for( j=0; j < i; ++j ) {
            A(j,i) = MAGMA_S_CONJ( A(i,j) );
        }
    }
}


// --------------------
// Make a matrix symmetric/symmetric positive definite.
// Increases diagonal by N, and makes it real.
// Sets Aji = conj( Aij ) for j < i, that is, copy lower triangle to upper triangle.
extern "C"
void magma_smake_hpd( magma_int_t N, float* A, magma_int_t lda )
{
    magma_int_t i, j;
    for( i=0; i < N; ++i ) {
        A(i,i) = MAGMA_S_MAKE( MAGMA_S_REAL( A(i,i) ) + N, 0. );
        for( j=0; j < i; ++j ) {
            A(j,i) = MAGMA_S_CONJ( A(i,j) );
        }
    }
}

#ifdef COMPLEX
// --------------------
// Make a matrix real-symmetric
// Dose NOT make diagonal real.
// Sets Aji = Aij for j < i, that is, copy lower triangle to upper triangle.
extern "C"
void magma_smake_symmetric( magma_int_t N, float* A, magma_int_t lda )
{
    magma_int_t i, j;
    for( i=0; i < N; ++i ) {
        for( j=0; j < i; ++j ) {
            A(j,i) =  A(i,j);
        }
    }
}


// --------------------
// Make a matrix real-symmetric positive definite.
// Increases diagonal by N. Does NOT make diagonal real.
// Sets Aji = Aij for j < i, that is, copy lower triangle to upper triangle.
extern "C"
void magma_smake_spd( magma_int_t N, float* A, magma_int_t lda )
{
    magma_int_t i, j;
    for( i=0; i < N; ++i ) {
        A(i,i) = MAGMA_S_MAKE( MAGMA_S_REAL( A(i,i) ) + N, MAGMA_S_IMAG( A(i,i) ) );
        for( j=0; j < i; ++j ) {
            A(j,i) = A(i,j);
        }
    }
}
#endif


// --------------------
// MKL 11.1 has bug in multi-threaded slansy; use single thread to work around.
// MKL 11.2 corrects it for inf, one, max norm.
// MKL 11.2 still segfaults for Frobenius norm.
// See testing_slansy.cpp
extern "C"
float safe_lapackf77_slansy(
    const char *norm, const char *uplo,
    const magma_int_t *n,
    const float *A, const magma_int_t *lda,
    float *work )
{
    #ifdef MAGMA_WITH_MKL
    // work around MKL bug in multi-threaded slansy
    magma_int_t la_threads = magma_get_lapack_numthreads();
    magma_set_lapack_numthreads( 1 );
    #endif
    
    float result = lapackf77_slansy( norm, uplo, n, A, lda, work );
    
    #ifdef MAGMA_WITH_MKL
    // end single thread to work around MKL bug
    magma_set_lapack_numthreads( la_threads );
    #endif
    
    return result;
}
