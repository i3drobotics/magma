/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zaxpy.cpp, normal z -> c, Sat Mar 27 20:31:46 2021
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "flops.h"
#include "magma_v2.h"
#include "magma_lapack.h"
#include "magma_operators.h"
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing caxpy
*/
int main(int argc, char **argv)
{
    #define  X(i_, j_)  ( X + (i_) + (j_)*lda)
    #define  Y(i_, j_)  ( Y + (i_) + (j_)*lda)
    
    #define dX(i_, j_)  (dX + (i_) + (j_)*ldda)
    #define dY(i_, j_)  (dY + (i_) + (j_)*ldda)
    
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    real_Double_t   gflops, dev_perf, dev_time, cpu_perf, cpu_time;
    float          dev_error, work[1];
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t M, N, lda, ldda, size;
    magma_int_t incx = 1;
    magma_int_t incy = 1;
    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    magmaFloatComplex alpha = MAGMA_C_MAKE(  1.5, -2.3 );
    magmaFloatComplex *X, *Y, *Yresult;
    magmaFloatComplex_ptr dX, dY;
    int status = 0;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );
    
    // Allow 3*eps; complex needs 2*sqrt(2) factor; see Higham, 2002, sec. 3.6.
    float eps = lapackf77_slamch("E");
    float tol = 3*eps;
    
    printf("%%   M   cnt     %s Gflop/s (ms)       CPU Gflop/s (ms)  %s error\n",
            g_platform_str, g_platform_str );
    printf("%%===========================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            // M is length of vector
            // N is number of vectors
            M = opts.msize[itest];
            N = 100;
            lda    = magma_roundup( M, 8 );  // multiple of 8 by default (64 byte cache line aligned)
            ldda   = magma_roundup( lda, opts.align );  // multiple of 32 by default
            gflops = 2*M*N / 1e9;
            size   = ldda*N;
            //printf( "m %d, n %d, lda %d, ldda %d, size %d\n", M, N, lda, ldda, size );
            
            TESTING_CHECK( magma_cmalloc_cpu( &X,       size ));
            TESTING_CHECK( magma_cmalloc_cpu( &Y,       size ));
            TESTING_CHECK( magma_cmalloc_cpu( &Yresult, size ));
            
            TESTING_CHECK( magma_cmalloc( &dX, size ));
            TESTING_CHECK( magma_cmalloc( &dY, size ));
            
            /* Initialize the matrix */
            lapackf77_clarnv( &ione, ISEED, &size, X );
            lapackf77_clarnv( &ione, ISEED, &size, Y );
            
            // for error checks
            float Xnorm = lapackf77_clange( "F", &M, &N, X, &lda, work );
            float Ynorm = lapackf77_clange( "F", &M, &N, Y, &lda, work );
            
            /* =====================================================================
               Performs operation using cuBLAS / clBLAS
               =================================================================== */
            magma_csetmatrix( M, N, X, lda, dX, ldda, opts.queue );
            magma_csetmatrix( M, N, Y, lda, dY, ldda, opts.queue );
            
            magma_flush_cache( opts.cache );
            dev_time = magma_sync_wtime( opts.queue );
            for (int j = 0; j < N; ++j) {
                magma_caxpy( M, alpha, dX(0,j), incx, dY(0,j), incy, opts.queue );
            }
            dev_time = magma_sync_wtime( opts.queue ) - dev_time;
            dev_perf = gflops / dev_time;
            
            magma_cgetmatrix( M, N, dY, ldda, Yresult, lda, opts.queue );
            
            /* =====================================================================
               Performs operation using CPU BLAS
               =================================================================== */
            magma_flush_cache( opts.cache );
            cpu_time = magma_wtime();
            for (int j = 0; j < N; ++j) {
                blasf77_caxpy( &M, &alpha, X(0,j), &incx, Y(0,j), &incy );
            }
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gflops / cpu_time;
            
            /* =====================================================================
               Check the result
               =================================================================== */
            // TODO: check error formula for axpy.
            // See testing_cgemm for formula. Here K = N.
            blasf77_caxpy( &size, &c_neg_one, Y, &ione, Yresult, &ione );
            dev_error = lapackf77_clange( "F", &M, &N, Yresult, &lda, work )
                            / (Xnorm + Ynorm);
            
            bool okay = (dev_error < tol);
            status += ! okay;
            printf("%5lld %5lld   %9.4f (%9.4f)   %9.4f (%9.4f)    %8.2e   %s\n",
                   (long long) M, (long long) N,
                   dev_perf,    1000.*dev_time,
                   cpu_perf,    1000.*cpu_time,
                   dev_error,
                   (okay ? "ok" : "failed"));
            
            magma_free_cpu( X );
            magma_free_cpu( Y );
            magma_free_cpu( Yresult );
            
            magma_free( dX );
            magma_free( dY );
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
