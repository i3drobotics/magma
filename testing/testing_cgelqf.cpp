/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zgelqf.cpp, normal z -> c, Sat Mar 27 20:32:07 2021

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

#define COMPLEX

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cgelqf
*/
int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    const float             d_neg_one = MAGMA_D_NEG_ONE;
    const float             d_one     = MAGMA_D_ONE;
    const magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    const magmaFloatComplex c_one     = MAGMA_C_ONE;
    const magmaFloatComplex c_zero    = MAGMA_C_ZERO;
    
    real_Double_t    gflops, gpu_perf, gpu_time, cpu_perf=0, cpu_time=0;
    float           Anorm, error=0, error2=0;
    magmaFloatComplex *h_A, *h_R, *tau, *h_work, tmp[1], unused[1];
    magma_int_t M, N, n2, lda, lwork, info, min_mn, nb;
    int status = 0;

    magma_opts opts;
    opts.parse_opts( argc, argv );

    float tol = opts.tolerance * lapackf77_slamch("E");
    
    printf("%%   M     N   CPU Gflop/s (sec)   GPU Gflop/s (sec)   |L - A*Q^H|   |I - Q*Q^H|\n");
    printf("%%==============================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[itest];
            N = opts.nsize[itest];
            min_mn = min(M, N);
            lda    = M;
            n2     = lda*N;
            nb     = magma_get_cgeqrf_nb( M, N );
            gflops = FLOPS_CGELQF( M, N ) / 1e9;

            // query for workspace size
            lwork = -1;
            lapackf77_cgelqf( &M, &N, unused, &M, unused, tmp, &lwork, &info );
            lwork = (magma_int_t)MAGMA_C_REAL( tmp[0] );
            lwork = max( lwork, M*nb );

            TESTING_CHECK( magma_cmalloc_cpu( &tau,    min_mn ));
            TESTING_CHECK( magma_cmalloc_cpu( &h_A,    n2     ));
            
            TESTING_CHECK( magma_cmalloc_pinned( &h_R,    n2     ));
            TESTING_CHECK( magma_cmalloc_pinned( &h_work, lwork  ));
            
            /* Initialize the matrix */
            magma_generate_matrix( opts, M, N, h_A, lda );
            lapackf77_clacpy( MagmaFullStr, &M, &N, h_A, &lda, h_R,  &lda );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_wtime();
            magma_cgelqf( M, N, h_R, lda, tau, h_work, lwork, &info);
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0) {
                printf("magma_cgelqf returned error %lld: %s.\n",
                       (long long) info, magma_strerror( info ));
            }

            /* =====================================================================
               Check the result, following zlqt01 except using the reduced Q.
               This works for any M,N (square, tall, wide).
               =================================================================== */
            if ( opts.check ) {
                magma_int_t ldq = min_mn;
                magma_int_t ldl = M;
                magmaFloatComplex *Q, *L;
                float *work;
                TESTING_CHECK( magma_cmalloc_cpu( &Q,    ldq*N ));       // K by N
                TESTING_CHECK( magma_cmalloc_cpu( &L,    ldl*min_mn ));  // M by K
                TESTING_CHECK( magma_smalloc_cpu( &work, min_mn ));
                
                // generate K by N matrix Q, where K = min(M,N)
                lapackf77_clacpy( "Upper", &min_mn, &N, h_R, &lda, Q, &ldq );
                lapackf77_cunglq( &min_mn, &N, &min_mn, Q, &ldq, tau, h_work, &lwork, &info );
                assert( info == 0 );
                
                // copy N by K matrix L
                lapackf77_claset( "Upper", &M, &min_mn, &c_zero, &c_zero, L, &ldl );
                lapackf77_clacpy( "Lower", &M, &min_mn, h_R, &lda,        L, &ldl );
                
                // error = || L - A*Q^H || / (N * ||A||)
                blasf77_cgemm( "NoTrans", "Conj", &M, &min_mn, &N,
                               &c_neg_one, h_A, &lda, Q, &ldq, &c_one, L, &ldl );
                Anorm = lapackf77_clange( "1", &M, &N,      h_A, &lda, work );
                error = lapackf77_clange( "1", &M, &min_mn, L,   &ldl, work );
                if ( N > 0 && Anorm > 0 )
                    error /= (N*Anorm);
                
                // set L = I (K by K), then L = I - Q*Q^H
                // error = || I - Q*Q^H || / N
                lapackf77_claset( "Upper", &min_mn, &min_mn, &c_zero, &c_one, L, &ldl );
                blasf77_cherk( "Upper", "NoTrans", &min_mn, &N, &d_neg_one, Q, &ldq, &d_one, L, &ldl );
                error2 = safe_lapackf77_clanhe( "1", "Upper", &min_mn, L, &ldl, work );
                if ( N > 0 )
                    error2 /= N;
                
                magma_free_cpu( Q    );  Q    = NULL;
                magma_free_cpu( L    );  L    = NULL;
                magma_free_cpu( work );  work = NULL;
            }
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                lapackf77_cgelqf( &M, &N, h_A, &lda, tau, h_work, &lwork, &info );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0) {
                    printf("lapack_cgelqf returned error %lld: %s.\n",
                           (long long) info, magma_strerror( info ));
                }
            }
            
            /* =====================================================================
               Print performance and error.
               =================================================================== */
            printf("%5lld %5lld   ", (long long) M, (long long) N );
            if ( opts.lapack ) {
                printf( "%7.2f (%7.2f)", cpu_perf, cpu_time );
            }
            else {
                printf("  ---   (  ---  )" );
            }
            printf( "   %7.2f (%7.2f)   ", gpu_perf, gpu_time );
            if ( opts.check ) {
                bool okay = (error < tol && error2 < tol);
                status += ! okay;
                printf( "%11.2e   %11.2e   %s\n", error, error2, (okay ? "ok" : "failed") );
            }
            else {
                printf( "    ---\n" );
            }
            
            magma_free_cpu( tau );
            magma_free_cpu( h_A );
            
            magma_free_pinned( h_R    );
            magma_free_pinned( h_work );
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
