/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date
       
       @author Stan Tomov
       @author Mark Gates

       @generated from testing/testing_zgeqr2x_gpu.cpp, normal z -> d, Sat Mar 27 20:32:05 2021
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
   -- Testing dgeqrf
*/
int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    /* Constants */
    double c_zero    = MAGMA_D_ZERO;
    double c_neg_one = MAGMA_D_NEG_ONE;
    double c_one     = MAGMA_D_ONE;
    double d_one     = MAGMA_D_ONE;
    double d_neg_one = MAGMA_D_NEG_ONE;
    
    /* Local variables */
    real_Double_t    gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    double           Anorm, error, error2, diff, terr, rwork[1];
    double *h_A, *h_T, *h_R, *tau, *h_work, tmp[1], unused[1];
    magmaDouble_ptr d_A, d_T, ddA, dtau;
    magmaDouble_ptr dwork;

    magma_int_t M, N, lda, ldda, lwork, size, info, min_mn;
    magma_int_t ione     = 1;
    int status = 0;

    #define BLOCK_SIZE 64

    magma_opts opts;
    opts.parse_opts( argc, argv );
    
    double tol = opts.tolerance * lapackf77_dlamch("E");
    
    printf("%% version %lld\n", (long long) opts.version );
    printf("%% It's okay if |Q - Q_magma| is large; MAGMA and LAPACK\n"
           "%% just chose different Householder reflectors, both valid.\n\n");
    
    printf("%%   M     N    CPU Gflop/s (ms)    GPU Gflop/s (ms)   |R - Q^H*A|   |I - Q^H*Q|   |T - T_magma|   |Q - Q_magma|\n");
    printf("%%==============================================================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M     = opts.msize[itest];
            N     = opts.nsize[itest];

            if (N > 128) {
                printf("%5lld %5lld   skipping because dgeqr2x requires N <= 128\n",
                        (long long) M, (long long) N);
                continue;
            }
            if (M < N) {
                printf("%5lld %5lld   skipping because dgeqr2x requires M >= N\n",
                        (long long) M, (long long) N);
                continue;
            }

            min_mn = min( M, N );
            lda    = M;
            ldda   = magma_roundup( M, opts.align );  // multiple of 32 by default
            // TODO: flops should be GEQRF + LARFT (whatever that is)
            gflops = (FLOPS_DGEQRF( M, N ) + FLOPS_DGEQRT( M, N )) / 1e9;

            lwork = -1;
            lapackf77_dgeqrf( &M, &N, unused, &M, unused, tmp, &lwork, &info );
            lwork = (magma_int_t)MAGMA_D_REAL( tmp[0] );
            lwork = max( lwork, N*N );
        
            /* Allocate memory for the matrix */
            TESTING_CHECK( magma_dmalloc_cpu( &tau,    min_mn ));
            TESTING_CHECK( magma_dmalloc_cpu( &h_A,    lda*N  ));
            TESTING_CHECK( magma_dmalloc_cpu( &h_T,    N*N    ));
            TESTING_CHECK( magma_dmalloc_cpu( &h_work, lwork ));
            
            TESTING_CHECK( magma_dmalloc_pinned( &h_R,    lda*N  ));
            
            TESTING_CHECK( magma_dmalloc( &d_A,    ldda*N ));
            TESTING_CHECK( magma_dmalloc( &d_T,    N*N    ));
            TESTING_CHECK( magma_dmalloc( &ddA,    N*N    ));
            TESTING_CHECK( magma_dmalloc( &dtau,   min_mn ));
            
            TESTING_CHECK( magma_dmalloc( &dwork,  max(5*min_mn, (BLOCK_SIZE*2+2)*min_mn) ));
            
            magmablas_dlaset( MagmaFull, N, N, c_zero, c_zero, ddA, N, opts.queue );
            magmablas_dlaset( MagmaFull, N, N, c_zero, c_zero, d_T, N, opts.queue );
            
            /* Initialize the matrix */
            magma_generate_matrix( opts, M, N, h_A, lda );
            lapackf77_dlacpy( MagmaFullStr, &M, &N, h_A, &lda, h_R, &lda );
            magma_dsetmatrix( M, N, h_R, lda, d_A, ldda, opts.queue );
    
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_sync_wtime( opts.queue );
    
            if (opts.version == 1) {
                magma_dgeqr2x_gpu(  M, N, d_A, ldda, dtau, d_T, ddA, dwork, &info );
            }
            else if (opts.version == 2) {
                magma_dgeqr2x2_gpu( M, N, d_A, ldda, dtau, d_T, ddA, dwork, &info );
            }
            else if (opts.version == 3) {
                magma_dgeqr2x3_gpu( M, N, d_A, ldda, dtau, d_T, ddA, dwork, &info );
            }
            else {
                /*
                  Going through NULL stream is faster
                  Going through any stream is slower
                  Doing two streams in parallel is slower than doing them sequentially
                  Queuing happens on the NULL stream - user defined buffers are smaller?
                */
                magma_dgeqr2x4_gpu( M, N, d_A, ldda, dtau, d_T, ddA, dwork, opts.queue, &info );
            }
            gpu_time = magma_sync_wtime( opts.queue ) - gpu_time;
            gpu_perf = gflops / gpu_time;

            if (info != 0) {
                printf("magma_dgeqr2x_gpu version %lld returned error %lld: %s.\n",
                       (long long) opts.version, (long long) info, magma_strerror( info ));
            }
            else if ( opts.check ) {
                /* =====================================================================
                   Check the result, following zqrt01 except using the reduced Q.
                   This works for any M,N (square, tall, wide).
                   =================================================================== */
                magma_dgetmatrix( M, N, d_A, ldda, h_R, lda, opts.queue );
                magma_dgetmatrix( N, N, ddA, N,    h_T, N, opts.queue );
                magma_dgetmatrix( min_mn, 1, dtau, min_mn, tau, min_mn, opts.queue );
                // Restore the upper triangular part of A before the check
                lapackf77_dlacpy( "Upper", &N, &N, h_T, &N, h_R, &lda );

                magma_int_t ldq = M;
                magma_int_t ldr = min_mn;
                double *Q, *R;
                double *work;
                TESTING_CHECK( magma_dmalloc_cpu( &Q,    ldq*min_mn ));  // M by K
                TESTING_CHECK( magma_dmalloc_cpu( &R,    ldr*N ));       // K by N
                TESTING_CHECK( magma_dmalloc_cpu( &work, min_mn ));
                
                // generate M by K matrix Q, where K = min(M,N)
                lapackf77_dlacpy( "Lower", &M, &min_mn, h_R, &lda, Q, &ldq );
                lapackf77_dorgqr( &M, &min_mn, &min_mn, Q, &ldq, tau, h_work, &lwork, &info );
                assert( info == 0 );
                
                // copy K by N matrix R
                lapackf77_dlaset( "Lower", &min_mn, &N, &c_zero, &c_zero, R, &ldr );
                lapackf77_dlacpy( "Upper", &min_mn, &N, h_R, &lda,        R, &ldr );
                
                // error = || R - Q^H*A || / (N * ||A||)
                blasf77_dgemm( "Conj", "NoTrans", &min_mn, &N, &M,
                               &c_neg_one, Q, &ldq, h_A, &lda, &c_one, R, &ldr );
                Anorm = lapackf77_dlange( "1", &M,      &N, h_A, &lda, work );
                error = lapackf77_dlange( "1", &min_mn, &N, R,   &ldr, work );
                if ( N > 0 && Anorm > 0 )
                    error /= (N*Anorm);
                
                // set R = I (K by K identity), then R = I - Q^H*Q
                // error = || I - Q^H*Q || / N
                lapackf77_dlaset( "Upper", &min_mn, &min_mn, &c_zero, &c_one, R, &ldr );
                blasf77_dsyrk( "Upper", "Conj", &min_mn, &M, &d_neg_one, Q, &ldq, &d_one, R, &ldr );
                error2 = safe_lapackf77_dlansy( "1", "Upper", &min_mn, R, &ldr, work );
                if ( N > 0 )
                    error2 /= N;
                
                magma_free_cpu( Q    );  Q    = NULL;
                magma_free_cpu( R    );  R    = NULL;
                magma_free_cpu( work );  work = NULL;

                /* =====================================================================
                   Performs operation using LAPACK
                   =================================================================== */
                cpu_time = magma_wtime();
                lapackf77_dgeqrf( &M, &N, h_A, &lda, tau, h_work, &lwork, &info );
                lapackf77_dlarft( MagmaForwardStr, MagmaColumnwiseStr,
                                  &M, &N, h_A, &lda, tau, h_work, &N );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0) {
                    printf("lapackf77_dgeqrf returned error %lld: %s.\n",
                           (long long) info, magma_strerror( info ));
                }

                /* =====================================================================
                   Check the result compared to LAPACK
                   Okay if these are different -- just chose different Householder reflectors
                   =================================================================== */
                size = lda*N;
                blasf77_daxpy( &size, &c_neg_one, h_A, &ione, h_R, &ione );
                Anorm = lapackf77_dlange( "M", &M, &N, h_A, &lda, rwork );
                diff =  lapackf77_dlange( "M", &M, &N, h_R, &lda, rwork );
                if ( Anorm > 0) {
                    diff /= (N*Anorm);
                }
                
                /* =====================================================================
                   Check if T is correct
                   =================================================================== */
                // Recompute T in h_work for d_A (magma), in case it is different than h_A (lapack)
                magma_dgetmatrix( M, N, d_A, ldda, h_R, lda, opts.queue );
                magma_dgetmatrix( min_mn, 1, dtau, min_mn, tau, min_mn, opts.queue );
                lapackf77_dlarft( MagmaForwardStr, MagmaColumnwiseStr,
                                  &M, &N, h_R, &lda, tau, h_work, &N );
                
                magma_dgetmatrix( N, N, d_T, N, h_T, N, opts.queue );
                size = N*N;
                blasf77_daxpy( &size, &c_neg_one, h_work, &ione, h_T, &ione );
                Anorm = lapackf77_dlantr( "F", "U", "N", &N, &N, h_work, &N, rwork );
                terr  = lapackf77_dlantr( "F", "U", "N", &N, &N, h_T,    &N, rwork );
                if (Anorm > 0) {
                    terr /= Anorm;
                }
                
                bool okay = (error < tol) && (error2 < tol) && (terr < tol);
                status += ! okay;
                printf("%5lld %5lld   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e      %8.2e      %8.2e        %8.2e   %s\n",
                       (long long) M, (long long) N, cpu_perf, 1000.*cpu_time, gpu_perf, 1000.*gpu_time,
                       error, error2, terr, diff,
                       (okay ? "ok" : "failed"));
            }
            else {
                printf("%5lld %5lld     ---   (  ---  )   %7.2f (%7.2f)     ---  \n",
                       (long long) M, (long long) N, gpu_perf, 1000.*gpu_time);
            }
            
            magma_free_cpu( tau    );
            magma_free_cpu( h_A    );
            magma_free_cpu( h_T    );
            magma_free_cpu( h_work );
            
            magma_free_pinned( h_R    );
        
            magma_free( d_A   );
            magma_free( d_T   );
            magma_free( ddA   );
            magma_free( dtau  );
            magma_free( dwork );
        
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
