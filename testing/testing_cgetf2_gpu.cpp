/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zgetf2_gpu.cpp, normal z -> c, Sat Mar 27 20:32:02 2021
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
#include "testings.h"


float get_LU_error(magma_int_t M, magma_int_t N,
                    magmaFloatComplex *A,  magma_int_t lda,
                    magmaFloatComplex *LU, magma_int_t *IPIV)
{
    magma_int_t min_mn = min(M,N);
    magma_int_t ione   = 1;
    magma_int_t i, j;
    magmaFloatComplex alpha = MAGMA_C_ONE;
    magmaFloatComplex beta  = MAGMA_C_ZERO;
    magmaFloatComplex *L, *U;
    float work[1], matnorm, residual;
    
    TESTING_CHECK( magma_cmalloc_cpu( &L, M*min_mn ));
    TESTING_CHECK( magma_cmalloc_cpu( &U, min_mn*N ));
    memset( L, 0, M*min_mn*sizeof(magmaFloatComplex) );
    memset( U, 0, min_mn*N*sizeof(magmaFloatComplex) );

    lapackf77_claswp( &N, A, &lda, &ione, &min_mn, IPIV, &ione);
    lapackf77_clacpy( MagmaLowerStr, &M, &min_mn, LU, &lda, L, &M      );
    lapackf77_clacpy( MagmaUpperStr, &min_mn, &N, LU, &lda, U, &min_mn );

    for (j=0; j < min_mn; j++)
        L[j+j*M] = MAGMA_C_MAKE( 1., 0. );
    
    matnorm = lapackf77_clange("f", &M, &N, A, &lda, work);

    blasf77_cgemm("N", "N", &M, &N, &min_mn,
                  &alpha, L, &M, U, &min_mn, &beta, LU, &lda);

    for( j = 0; j < N; j++ ) {
        for( i = 0; i < M; i++ ) {
            LU[i+j*lda] = MAGMA_C_SUB( LU[i+j*lda], A[i+j*lda] );
        }
    }
    residual = lapackf77_clange("f", &M, &N, LU, &lda, work);

    magma_free_cpu( L );
    magma_free_cpu( U );

    return residual / (matnorm * N);
}

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cgetrf
*/
int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    real_Double_t   gflops, gpu_perf, gpu_time, cpu_perf=0, cpu_time=0;
    float          error;
    magmaFloatComplex *h_A, *h_R;
    magmaFloatComplex_ptr d_A;
    magma_int_t     *ipiv;
    magma_int_t M, N, n2, lda, ldda, info, min_mn;
    int status = 0;

    magma_opts opts;
    opts.parse_opts( argc, argv );

    float tol = opts.tolerance * lapackf77_slamch("E");
    
    printf("%%   M     N   CPU Gflop/s (ms)    GPU Gflop/s (ms)  Copy time (ms)  ||PA-LU||/(||A||*N)\n");
    printf("%%======================================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[itest];
            N = opts.nsize[itest];
            min_mn = min(M, N);
            lda    = M;
            n2     = lda*N;
            ldda   = magma_roundup( M, opts.align );  // multiple of 32 by default
            gflops = FLOPS_CGETRF( M, N ) / 1e9;
            
            if ( N > 512 ) {
                printf( "%5lld %5lld   skipping because cgetf2 does not support N > 512\n", (long long) M, (long long) N );
                continue;
            }
            
            TESTING_CHECK( magma_imalloc_cpu( &ipiv, min_mn ));
            TESTING_CHECK( magma_cmalloc_cpu( &h_A,  n2     ));
            TESTING_CHECK( magma_cmalloc_pinned( &h_R,  n2     ));
            TESTING_CHECK( magma_cmalloc( &d_A,  ldda*N ));
            
            /* Initialize the matrix */
            magma_generate_matrix( opts, M, N, h_A, lda );
            lapackf77_clacpy( MagmaFullStr, &M, &N, h_A, &lda, h_R, &lda );

            real_Double_t set_time = magma_wtime();
            magma_csetmatrix( M, N, h_R, lda, d_A, ldda, opts.queue );
            set_time =  magma_wtime() - set_time;

            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                lapackf77_cgetrf( &M, &N, h_A, &lda, ipiv, &info );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0) {
                    printf("lapackf77_cgetrf returned error %lld: %s.\n",
                           (long long) info, magma_strerror( info ));
                }
            }
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_sync_wtime( opts.queue );
            magma_cgetf2_gpu( M, N, d_A, ldda, ipiv, opts.queue, &info );
            gpu_time = magma_sync_wtime( opts.queue ) - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0) {
                printf("magma_cgetf2_gpu returned error %lld: %s.\n",
                       (long long) info, magma_strerror( info ));
            }
            
            real_Double_t get_time = magma_wtime();
            magma_cgetmatrix( M, N, d_A, ldda, h_A, lda, opts.queue );
            get_time =  magma_wtime() - get_time;

            /* =====================================================================
               Check the factorization
               =================================================================== */
            if ( opts.lapack ) {
                printf("%5lld %5lld   %7.2f (%7.2f)   %7.2f (%7.2f)   %7.2f",
                       (long long) M, (long long) N, cpu_perf, cpu_time*1000., gpu_perf, gpu_time*1000.,
                       set_time*1000.+get_time*1000.);
            }
            else {
                printf("%5lld %5lld     ---   (  ---  )   %7.2f (%7.2f)   %7.2f",
                       (long long) M, (long long) N, gpu_perf, gpu_time*1000., set_time*1000.+get_time*1000. );
            }
            if ( opts.check ) {
                magma_cgetmatrix( M, N, d_A, ldda, h_A, lda, opts.queue );
                error = get_LU_error( M, N, h_R, lda, h_A, ipiv );
                printf("   %8.2e   %s\n", error, (error < tol ? "ok" : "failed") );
                status += ! (error < tol);
            }
            else {
                printf("     ---  \n");
            }
            
            magma_free_cpu( ipiv );
            magma_free_cpu( h_A );
            magma_free_pinned( h_R );
            magma_free( d_A );
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
