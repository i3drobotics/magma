/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zgeadd.cpp, normal z -> c, Sat Mar 27 20:31:52 2021
       @author Mark Gates
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "magma_v2.h"
#include "magma_lapack.h"
#include "magma_operators.h"
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cgeadd
*/
int main( int argc, char** argv)
{
    #define h_A(i_, j_) (h_A + (i_) + (j_)*lda)
    #define h_B(i_, j_) (h_B + (i_) + (j_)*lda)  // B uses lda
    
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    real_Double_t   gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    float          Bnorm, error, work[1];
    magmaFloatComplex *h_A, *h_B, *d_A, *d_B;
    magmaFloatComplex alpha = MAGMA_C_MAKE( 3.1415, 2.71828 );
    magmaFloatComplex beta  = MAGMA_C_MAKE( 6.0221, 6.67408 );
    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    
    magma_int_t M, N, size, lda, ldda;
    magma_int_t ione = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    int status = 0;

    magma_opts opts;
    opts.parse_opts( argc, argv );

    float tol = opts.tolerance * lapackf77_slamch("E");
    
    /* Uncomment these lines to check parameters.
     * magma_xerbla calls lapack's xerbla to print out error. */
    //magmablas_cgeadd( -1,  N, alpha, d_A, ldda, d_B, ldda, opts.queue );
    //magmablas_cgeadd(  M, -1, alpha, d_A, ldda, d_B, ldda, opts.queue );
    //magmablas_cgeadd(  M,  N, alpha, d_A, M-1,  d_B, ldda, opts.queue );
    //magmablas_cgeadd(  M,  N, alpha, d_A, ldda, d_B, N-1,  opts.queue );

    printf("%%   M     N   CPU Gflop/s (ms)    GPU Gflop/s (ms)    |Bl-Bm|/|Bl|\n");
    printf("%%========================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[itest];
            N = opts.nsize[itest];
            lda    = M;
            ldda   = magma_roundup( M, opts.align );  // multiple of 32 by default
            size   = lda*N;
            gflops = 2.*M*N / 1e9;
            
            TESTING_CHECK( magma_cmalloc_cpu( &h_A, lda *N ));
            TESTING_CHECK( magma_cmalloc_cpu( &h_B, lda *N ));
            
            TESTING_CHECK( magma_cmalloc( &d_A, ldda*N ));
            TESTING_CHECK( magma_cmalloc( &d_B, ldda*N ));
            
            lapackf77_clarnv( &ione, ISEED, &size, h_A );
            lapackf77_clarnv( &ione, ISEED, &size, h_B );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            magma_csetmatrix( M, N, h_A, lda, d_A, ldda, opts.queue );
            magma_csetmatrix( M, N, h_B, lda, d_B, ldda, opts.queue );
            
            gpu_time = magma_sync_wtime( opts.queue );
            if ( opts.version == 1 ) {
                magmablas_cgeadd( M, N, alpha, d_A, ldda, d_B, ldda, opts.queue );
            }
            else {
                magmablas_cgeadd2( M, N, alpha, d_A, ldda, beta, d_B, ldda, opts.queue );
            }
            gpu_time = magma_sync_wtime( opts.queue ) - gpu_time;
            gpu_perf = gflops / gpu_time;
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            cpu_time = magma_wtime();
            if ( opts.version == 1 ) {
            for( int j = 0; j < N; ++j ) {
                blasf77_caxpy( &M, &alpha, &h_A[j*lda], &ione, &h_B[j*lda], &ione );
            }
            }
            else {
                for( int j = 0; j < N; ++j ) {
                    // daxpby
                    for( int i=0; i < M; ++i ) {
                        *h_B(i,j) = alpha * (*h_A(i,j)) + beta * (*h_B(i,j));
                    }
                }
            }
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gflops / cpu_time;
            
            /* =====================================================================
               Check result
               =================================================================== */
            magma_cgetmatrix( M, N, d_B, ldda, h_A, lda, opts.queue );
            
            blasf77_caxpy( &size, &c_neg_one, h_B, &ione, h_A, &ione );
            Bnorm = lapackf77_clange( "F", &M, &N, h_B, &lda, work );
            error = lapackf77_clange( "F", &M, &N, h_A, &lda, work ) / Bnorm;
            
            printf("%5lld %5lld   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s\n",
                   (long long) M, (long long) N,
                   cpu_perf, cpu_time*1000., gpu_perf, gpu_time*1000.,
                   error, (error < tol ? "ok" : "failed"));
            status += ! (error < tol);
            
            magma_free_cpu( h_A );
            magma_free_cpu( h_B );
            
            magma_free( d_A );
            magma_free( d_B );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    opts.cleanup();
    TESTING_CHECK( magma_finalize() );
    return status;
}
