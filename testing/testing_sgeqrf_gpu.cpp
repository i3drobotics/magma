/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zgeqrf_gpu.cpp, normal z -> s, Sat Mar 27 20:32:06 2021
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

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing sgeqrf
*/
int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    const float             d_neg_one = MAGMA_D_NEG_ONE;
    const float             d_one     = MAGMA_D_ONE;
    const float c_neg_one = MAGMA_S_NEG_ONE;
    const float c_one     = MAGMA_S_ONE;
    const float c_zero    = MAGMA_S_ZERO;
    const magma_int_t        ione      = 1;
    
    real_Double_t    gflops, gpu_perf, gpu_time, cpu_perf=0, cpu_time=0;
    float           Anorm, error=0, error2=0;
    float *h_A, *h_R, *tau, *h_work, tmp[1], unused[1];
    magmaFloat_ptr d_A, dT;
    magma_int_t M, N, n2, lda, ldda, lwork, info, min_mn, nb, size;
    magma_int_t ISEED[4] = {0,0,0,1};
    
    magma_opts opts;
    opts.parse_opts( argc, argv );
    
    int status = 0;
    float tol = opts.tolerance * lapackf77_slamch("E");
    
    // version 3 can do either check
    if (opts.check == 1 && opts.version == 1) {
        opts.check = 2;
        printf( "%% version 1 requires check 2 (solve A*x=b)\n" );
    }
    if (opts.check == 2 && opts.version == 2) {
        opts.check = 1;
        printf( "%% version 2 requires check 1 (R - Q^H*A)\n" );
    }
    
    printf( "%% version %lld\n", (long long) opts.version );
    if ( opts.check == 1 ) {
        printf("%%   M     N   CPU Gflop/s (sec)   GPU Gflop/s (sec)   |R - Q^H*A|   |I - Q^H*Q|\n");
        printf("%%==============================================================================\n");
    }
    else {
        printf("%%   M     N   CPU Gflop/s (sec)   GPU Gflop/s (sec)    |b - A*x|\n");
        printf("%%===============================================================\n");
    }
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[itest];
            N = opts.nsize[itest];
            min_mn = min( M, N );
            lda    = M;
            n2     = lda*N;
            ldda   = magma_roundup( M, opts.align );  // multiple of 32 by default
            nb     = magma_get_sgeqrf_nb( M, N );
            gflops = FLOPS_SGEQRF( M, N ) / 1e9;
            
            // query for workspace size
            lwork = -1;
            lapackf77_sgeqrf( &M, &N, unused, &M, unused, tmp, &lwork, &info );
            lwork = (magma_int_t)MAGMA_S_REAL( tmp[0] );
            
            TESTING_CHECK( magma_smalloc_cpu( &tau,    min_mn ));
            TESTING_CHECK( magma_smalloc_cpu( &h_A,    n2     ));
            TESTING_CHECK( magma_smalloc_cpu( &h_work, lwork  ));
            
            TESTING_CHECK( magma_smalloc_pinned( &h_R,    n2     ));
            
            TESTING_CHECK( magma_smalloc( &d_A,    ldda*N ));
            
            if ( opts.version == 1 || opts.version == 3 ) {
                size = (2*min(M, N) + magma_roundup( N, 32 ) )*nb;
                TESTING_CHECK( magma_smalloc( &dT, size ));
                magmablas_slaset( MagmaFull, size, 1, c_zero, c_zero, dT, size, opts.queue );
            }
            
            /* Initialize the matrix */
            magma_generate_matrix( opts, M, N, h_A, lda );
            lapackf77_slacpy( MagmaFullStr, &M, &N, h_A, &lda, h_R, &lda );
            magma_ssetmatrix( M, N, h_R, lda, d_A, ldda, opts.queue );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            nb = magma_get_sgeqrf_nb( M, N );
            
            gpu_time = magma_wtime();
            if ( opts.version == 1 ) {
                // stores dT, V blocks have zeros, R blocks inverted & stored in dT
                magma_sgeqrf_gpu( M, N, d_A, ldda, tau, dT, &info );
            }
            else if ( opts.version == 2 ) {
                // LAPACK complaint arguments
                magma_sgeqrf2_gpu( M, N, d_A, ldda, tau, &info );
            }
            #if defined(HAVE_CUBLAS) || defined(HAVE_HIP)
            else if ( opts.version == 3 ) {
                // stores dT, V blocks have zeros, R blocks stored in dT
                magma_sgeqrf3_gpu( M, N, d_A, ldda, tau, dT, &info );
            }
            #endif
            else {
                printf( "Unknown version %lld\n", (long long) opts.version );
                return -1;
            }
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0) {
                printf("magma_sgeqrf returned error %lld: %s.\n",
                       (long long) info, magma_strerror( info ));
            }
            
            if ( opts.check == 1 && (opts.version == 2 || opts.version == 3) ) {
                if ( opts.version == 3 ) {
                    // copy diagonal blocks of R back to A
                    for( int i=0; i < min_mn-nb; i += nb ) {
                        magma_int_t ib = min( min_mn-i, nb );
                        magmablas_slacpy( MagmaUpper, ib, ib, &dT[min_mn*nb + i*nb], nb, &d_A[ i + i*ldda ], ldda, opts.queue );
                    }
                }
                
                /* =====================================================================
                   Check the result, following zqrt01 except using the reduced Q.
                   This works for any M,N (square, tall, wide).
                   Only for version 2, which has LAPACK complaint output.
                   Or   for version 3, after restoring diagonal blocks of A above.
                   =================================================================== */
                magma_sgetmatrix( M, N, d_A, ldda, h_R, lda, opts.queue );
                
                magma_int_t ldq = M;
                magma_int_t ldr = min_mn;
                float *Q, *R;
                float *work;
                TESTING_CHECK( magma_smalloc_cpu( &Q,    ldq*min_mn ));  // M by K
                TESTING_CHECK( magma_smalloc_cpu( &R,    ldr*N ));       // K by N
                TESTING_CHECK( magma_smalloc_cpu( &work, min_mn ));
                
                // generate M by K matrix Q, where K = min(M,N)
                lapackf77_slacpy( "Lower", &M, &min_mn, h_R, &lda, Q, &ldq );
                lapackf77_sorgqr( &M, &min_mn, &min_mn, Q, &ldq, tau, h_work, &lwork, &info );
                assert( info == 0 );
                
                // copy K by N matrix R
                lapackf77_slaset( "Lower", &min_mn, &N, &c_zero, &c_zero, R, &ldr );
                lapackf77_slacpy( "Upper", &min_mn, &N, h_R, &lda,        R, &ldr );
                
                // error = || R - Q^H*A || / (N * ||A||)
                blasf77_sgemm( "Conj", "NoTrans", &min_mn, &N, &M,
                               &c_neg_one, Q, &ldq, h_A, &lda, &c_one, R, &ldr );
                Anorm = lapackf77_slange( "1", &M,      &N, h_A, &lda, work );
                error = lapackf77_slange( "1", &min_mn, &N, R,   &ldr, work );
                if ( N > 0 && Anorm > 0 )
                    error /= (N*Anorm);
                
                // set R = I (K by K identity), then R = I - Q^H*Q
                // error = || I - Q^H*Q || / N
                lapackf77_slaset( "Upper", &min_mn, &min_mn, &c_zero, &c_one, R, &ldr );
                blasf77_ssyrk( "Upper", "Conj", &min_mn, &M, &d_neg_one, Q, &ldq, &d_one, R, &ldr );
                error2 = safe_lapackf77_slansy( "1", "Upper", &min_mn, R, &ldr, work );
                if ( N > 0 )
                    error2 /= N;
                
                magma_free_cpu( Q    );  Q    = NULL;
                magma_free_cpu( R    );  R    = NULL;
                magma_free_cpu( work );  work = NULL;
            }
            else if ( opts.check == 2 && M >= N && (opts.version == 1 || opts.version == 3) ) {
                /* =====================================================================
                   Check the result by solving consistent linear system, A*x = b.
                   Only for versions 1 & 3 with M >= N.
                   =================================================================== */
                magma_int_t lwork2;
                float *x, *b, *hwork;
                magmaFloat_ptr d_B;

                // initialize RHS, b = A*random
                TESTING_CHECK( magma_smalloc_cpu( &x, N ));
                TESTING_CHECK( magma_smalloc_cpu( &b, M ));
                lapackf77_slarnv( &ione, ISEED, &N, x );
                blasf77_sgemv( "Notrans", &M, &N, &c_one, h_A, &lda, x, &ione, &c_zero, b, &ione );
                // copy to GPU
                TESTING_CHECK( magma_smalloc( &d_B, M ));
                magma_ssetvector( M, b, 1, d_B, 1, opts.queue );

                if ( opts.version == 1 ) {
                    // allocate hwork
                    magma_sgeqrs_gpu( M, N, 1,
                                      d_A, ldda, tau, dT,
                                      d_B, M, tmp, -1, &info );
                    lwork2 = (magma_int_t)MAGMA_S_REAL( tmp[0] );
                    TESTING_CHECK( magma_smalloc_cpu( &hwork, lwork2 ));

                    // solve linear system
                    magma_sgeqrs_gpu( M, N, 1,
                                      d_A, ldda, tau, dT,
                                      d_B, M, hwork, lwork2, &info );
                    if (info != 0) {
                        printf("magma_sgeqrs returned error %lld: %s.\n",
                               (long long) info, magma_strerror( info ));
                    }
                    magma_free_cpu( hwork );
                }
                #if defined(HAVE_CUBLAS) || defined(HAVE_HIP)
                else if ( opts.version == 3 ) {
                    // allocate hwork
                    magma_sgeqrs3_gpu( M, N, 1,
                                       d_A, ldda, tau, dT,
                                       d_B, M, tmp, -1, &info );
                    lwork2 = (magma_int_t)MAGMA_S_REAL( tmp[0] );
                    TESTING_CHECK( magma_smalloc_cpu( &hwork, lwork2 ));

                    // solve linear system
                    magma_sgeqrs3_gpu( M, N, 1,
                                       d_A, ldda, tau, dT,
                                       d_B, M, hwork, lwork2, &info );
                    if (info != 0) {
                        printf("magma_sgeqrs3 returned error %lld: %s.\n",
                               (long long) info, magma_strerror( info ));
                    }
                    magma_free_cpu( hwork );
                }
                #endif
                else {
                    printf( "Unknown version %lld\n", (long long) opts.version );
                    return -1;
                }
                magma_sgetvector( N, d_B, 1, x, 1, opts.queue );

                // compute r = Ax - b, saved in b
                blasf77_sgemv( "Notrans", &M, &N, &c_one, h_A, &lda, x, &ione, &c_neg_one, b, &ione );

                // compute residual |Ax - b| / (max(m,n)*|A|*|x|)
                float norm_x, norm_A, norm_r, work[1];
                norm_A = lapackf77_slange( "F", &M, &N, h_A, &lda, work );
                norm_r = lapackf77_slange( "F", &M, &ione, b, &M, work );
                norm_x = lapackf77_slange( "F", &N, &ione, x, &N, work );

                magma_free_cpu( x );
                magma_free_cpu( b );
                magma_free( d_B );

                error = norm_r / (max(M,N) * norm_A * norm_x);
            }
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                lapackf77_sgeqrf( &M, &N, h_A, &lda, tau, h_work, &lwork, &info );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0) {
                    printf("lapackf77_sgeqrf returned error %lld: %s.\n",
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
            if ( opts.check == 1 ) {
                bool okay = (error < tol && error2 < tol);
                status += ! okay;
                printf( "%11.2e   %11.2e   %s\n", error, error2, (okay ? "ok" : "failed") );
            }
            else if ( opts.check == 2 ) {
                if ( M >= N ) {
                    bool okay = (error < tol);
                    status += ! okay;
                    printf( "%10.2e   %s\n", error, (okay ? "ok" : "failed") );
                }
                else {
                    printf( "(error check only for M >= N)\n" );
                }
            }
            else {
                printf( "    ---\n" );
            }
            
            magma_free_cpu( tau    );
            magma_free_cpu( h_A    );
            magma_free_cpu( h_work );
            
            magma_free_pinned( h_R );
            
            magma_free( d_A );
            
            if ( opts.version == 1 || opts.version == 3 ) {
                magma_free( dT );
            }
            
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
