/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zgeadd_batched.cpp, normal z -> s, Sat Mar 27 20:32:15 2021
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
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing sgeadd_batched
   Code is very similar to testing_slacpy_batched.cpp
*/
int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    real_Double_t    gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    float           error, norm, work[1];
    float  c_neg_one = MAGMA_S_NEG_ONE;
    float *h_A, *h_B;
    magmaFloat_ptr d_A, d_B;
    float **hAarray, **hBarray, **dAarray, **dBarray;
    float alpha = MAGMA_S_MAKE( 3.1415, 2.718 );
    magma_int_t j, M, N, mb, nb, size, lda, ldda, mstride, nstride, ntile, offset, tile;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    int status = 0;
    
    magma_opts opts( MagmaOptsBatched );
    opts.parse_opts( argc, argv );

    float tol = opts.tolerance * lapackf77_slamch("E");
    mb = (opts.nb == 0 ? 32 : opts.nb);
    nb = (opts.nb == 0 ? 64 : opts.nb);
    mstride = 2*mb;
    nstride = 3*nb;
    
    printf("%% mb=%lld, nb=%lld, mstride=%lld, nstride=%lld\n", (long long) mb, (long long) nb, (long long) mstride, (long long) nstride );
    printf("%%   M     N ntile   CPU Gflop/s (ms)    GPU Gflop/s (ms)    error   \n");
    printf("%%===================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[itest];
            N = opts.nsize[itest];
            lda    = M;
            ldda   = magma_roundup( M, opts.align );  // multiple of 32 by default
            size   = lda*N;
            
            if ( N < nb || M < nb ) {
                ntile = 0;
            } else {
                ntile = min( (M - nb)/mstride + 1,
                             (N - nb)/nstride + 1 );
            }
            gflops = 2.*mb*nb*ntile / 1e9;
            
            TESTING_CHECK( magma_smalloc_cpu( &h_A, lda *N ));
            TESTING_CHECK( magma_smalloc_cpu( &h_B, lda *N ));
            TESTING_CHECK( magma_smalloc( &d_A, ldda*N ));
            TESTING_CHECK( magma_smalloc( &d_B, ldda*N ));
            
            TESTING_CHECK( magma_malloc_cpu( (void**) &hAarray, ntile * sizeof(float*) ));
            TESTING_CHECK( magma_malloc_cpu( (void**) &hBarray, ntile * sizeof(float*) ));
            TESTING_CHECK( magma_malloc( (void**) &dAarray, ntile * sizeof(float*) ));
            TESTING_CHECK( magma_malloc( (void**) &dBarray, ntile * sizeof(float*) ));
            
            lapackf77_slarnv( &ione, ISEED, &size, h_A );
            lapackf77_slarnv( &ione, ISEED, &size, h_B );

            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            magma_ssetmatrix( M, N, h_A, lda, d_A, ldda, opts.queue );
            magma_ssetmatrix( M, N, h_B, lda, d_B, ldda, opts.queue );
            
            // setup pointers
            for( tile = 0; tile < ntile; ++tile ) {
                offset = tile*mstride + tile*nstride*ldda;
                hAarray[tile] = &d_A[offset];
                hBarray[tile] = &d_B[offset];
            }
            magma_setvector( ntile, sizeof(float*), hAarray, 1, dAarray, 1, opts.queue );
            magma_setvector( ntile, sizeof(float*), hBarray, 1, dBarray, 1, opts.queue );
            
            gpu_time = magma_sync_wtime( opts.queue );
            magmablas_sgeadd_batched( mb, nb, alpha, dAarray, ldda, dBarray, ldda, ntile, opts.queue );
            gpu_time = magma_sync_wtime( opts.queue ) - gpu_time;
            gpu_perf = gflops / gpu_time;
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            cpu_time = magma_wtime();
            for( tile = 0; tile < ntile; ++tile ) {
                offset = tile*mstride + tile*nstride*lda;
                for( j = 0; j < nb; ++j ) {
                    blasf77_saxpy( &mb, &alpha,
                                   &h_A[offset + j*lda], &ione,
                                   &h_B[offset + j*lda], &ione );
                }
            }
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gflops / cpu_time;
            
            /* =====================================================================
               Check the result
               =================================================================== */
            magma_sgetmatrix( M, N, d_B, ldda, h_A, lda, opts.queue );
            
            norm  = lapackf77_slange( "F", &M, &N, h_B, &lda, work );
            blasf77_saxpy(&size, &c_neg_one, h_A, &ione, h_B, &ione);
            error = lapackf77_slange("f", &M, &N, h_B, &lda, work) / norm;
            bool okay = (error < tol);
            status += ! okay;

            printf("%5lld %5lld %5lld   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s\n",
                   (long long) M, (long long) N, (long long) ntile,
                   cpu_perf, cpu_time*1000., gpu_perf, gpu_time*1000.,
                   error, (okay ? "ok" : "failed"));
            
            magma_free_cpu( h_A );
            magma_free_cpu( h_B );
            magma_free( d_A );
            magma_free( d_B );
            
            magma_free_cpu( hAarray );
            magma_free_cpu( hBarray );
            magma_free( dAarray );
            magma_free( dBarray );
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
