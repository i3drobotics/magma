/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Mark Gates
       @generated from testing/testing_zunmqr.cpp, normal z -> c, Sat Mar 27 20:32:10 2021
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// includes, project
#include "flops.h"
#include "magma_v2.h"
#include "magma_lapack.h"
#include "magma_operators.h"
#include "testings.h"

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cunmqr
*/
int main( int argc, char** argv )
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();
    
    real_Double_t   gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    float Cnorm, error, work[1];
    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    magma_int_t ione = 1;
    magma_int_t mm, m, n, k, size, info;
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t nb, ldc, lda, lwork, lwork_max;
    magmaFloatComplex *C, *R, *A, *W, *tau;
    int status = 0;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );
    
    // need slightly looser bound (60*eps instead of 30*eps) for some tests
    opts.tolerance = max( 60., opts.tolerance );
    float tol = opts.tolerance * lapackf77_slamch("E");
    
    // pass ngpu = -1 to test multi-GPU code using 1 gpu
    magma_int_t abs_ngpu = abs( opts.ngpu );
    
    // test all combinations of input parameters
    magma_side_t  side [] = { MagmaLeft,       MagmaRight   };
    magma_trans_t trans[] = { Magma_ConjTrans, MagmaNoTrans };

    printf("%%   M     N     K   side   trans   CPU Gflop/s (sec)   GPU Gflop/s (sec)   ||R||_F / ||QC||_F\n");
    printf("%%==============================================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
      for( int iside = 0; iside < 2; ++iside ) {
      for( int itran = 0; itran < 2; ++itran ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            m = opts.msize[itest];
            n = opts.nsize[itest];
            k = opts.ksize[itest];
            nb  = magma_get_cgeqrf_nb( m, n );
            ldc = m;
            // A is mm x k == m x k (left) or n x k (right)
            mm = (side[iside] == MagmaLeft ? m : n);
            lda = mm;
            gflops = FLOPS_CUNMQR( m, n, k, side[iside] ) / 1e9;
            
            if ( side[iside] == MagmaLeft && m < k ) {
                printf( "%5lld %5lld %5lld   %4c   %5c   skipping because side=left  and m < k\n",
                        (long long) m, (long long) n, (long long) k,
                        lapacke_side_const( side[iside] ),
                        lapacke_trans_const( trans[itran] ) );
                continue;
            }
            if ( side[iside] == MagmaRight && n < k ) {
                printf( "%5lld %5lld %5lld   %4c   %5c   skipping because side=right and n < k\n",
                        (long long) m, (long long) n, (long long) k,
                        lapacke_side_const( side[iside] ),
                        lapacke_trans_const( trans[itran] ) );
                continue;
            }
            
            // need at least 2*nb*nb for geqrf
            lwork_max = max( max( m*nb, n*nb ), 2*nb*nb );
            // this rounds it up slightly if needed to agree with lwork query below
            lwork_max = magma_int_t( real( magma_cmake_lwork( lwork_max )));
            
            TESTING_CHECK( magma_cmalloc_cpu( &C,   ldc*n ));
            TESTING_CHECK( magma_cmalloc_cpu( &R,   ldc*n ));
            TESTING_CHECK( magma_cmalloc_cpu( &A,   lda*k ));
            TESTING_CHECK( magma_cmalloc_cpu( &W,   lwork_max ));
            TESTING_CHECK( magma_cmalloc_cpu( &tau, k ));
            
            // C is full, m x n
            size = ldc*n;
            lapackf77_clarnv( &ione, ISEED, &size, C );
            lapackf77_clacpy( "Full", &m, &n, C, &ldc, R, &ldc );
            
            // A is mm x k
            magma_generate_matrix( opts, mm, k, A, lda );
            
            // compute QR factorization to get Householder vectors in A, tau
            magma_cgeqrf( mm, k, A, lda, tau, W, lwork_max, &info );
            if (info != 0) {
                printf("magma_cgeqrf returned error %lld: %s.\n",
                       (long long) info, magma_strerror( info ));
            }
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            cpu_time = magma_wtime();
            lapackf77_cunmqr( lapack_side_const( side[iside] ), lapack_trans_const( trans[itran] ),
                              &m, &n, &k,
                              A, &lda, tau, C, &ldc, W, &lwork_max, &info );
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gflops / cpu_time;
            if (info != 0) {
                printf("lapackf77_cunmqr returned error %lld: %s.\n",
                       (long long) info, magma_strerror( info ));
            }
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            // query for workspace size
            lwork = -1;
            magma_cunmqr( side[iside], trans[itran],
                          m, n, k,
                          A, lda, tau, R, ldc, W, lwork, &info );
            if (info != 0) {
                printf("magma_cunmqr (lwork query) returned error %lld: %s.\n",
                       (long long) info, magma_strerror( info ));
            }
            lwork = (magma_int_t) MAGMA_C_REAL( W[0] );
            if ( lwork < 0 || lwork > lwork_max ) {
                printf("Warning: optimal lwork %lld > allocated lwork_max %lld\n", (long long) lwork, (long long) lwork_max );
                lwork = lwork_max;
            }
            
            gpu_time = magma_wtime();
            if ( opts.ngpu == 1 ) {
                magma_cunmqr( side[iside], trans[itran],
                              m, n, k,
                              A, lda, tau, R, ldc, W, lwork, &info );
            }
            else {
                if ( side[iside] == MagmaLeft ) {
                    magma_cunmqr_m( abs_ngpu, side[iside], trans[itran],
                                    m, n, k,
                                    A, lda, tau, R, ldc, W, lwork, &info );
                }
                else {
                    printf( "%5lld %5lld %5lld   %4c   %5c   skipping because magma_cunmqr_m doesn't support MagmaRight\n",
                            (long long) m, (long long) n, (long long) k,
                            lapacke_side_const( side[iside] ),
                            lapacke_trans_const( trans[itran] ) );
                    goto cleanup;
                }
            }
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0) {
                printf("magma_cunmqr returned error %lld: %s.\n",
                       (long long) info, magma_strerror( info ));
            }
            
            /* =====================================================================
               compute relative error |QC_magma - QC_lapack| / |QC_lapack|
               =================================================================== */
            size = ldc*n;
            blasf77_caxpy( &size, &c_neg_one, C, &ione, R, &ione );
            Cnorm = lapackf77_clange( "Fro", &m, &n, C, &ldc, work );
            error = lapackf77_clange( "Fro", &m, &n, R, &ldc, work ) / (magma_ssqrt(m*n) * Cnorm);
            
            printf( "%5lld %5lld %5lld   %4c   %5c   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s\n",
                    (long long) m, (long long) n, (long long) k,
                    lapacke_side_const( side[iside] ),
                    lapacke_trans_const( trans[itran] ),
                    cpu_perf, cpu_time, gpu_perf, gpu_time,
                    error, (error < tol ? "ok" : "failed") );
            status += ! (error < tol);
            
        cleanup:
            magma_free_cpu( C );
            magma_free_cpu( R );
            magma_free_cpu( A );
            magma_free_cpu( W );
            magma_free_cpu( tau );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
      }}  // end iside, itran
      printf( "\n" );
    }
    
    opts.cleanup();
    TESTING_CHECK( magma_finalize() );
    return status;
}
