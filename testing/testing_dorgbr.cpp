/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zungbr.cpp, normal z -> d, Sat Mar 27 20:32:14 2021

       @author Stan Tomov
       @author Mathieu Faverge
       @author Mark Gates
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
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing dorgbr
*/
int main( int argc, char** argv )
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    real_Double_t    gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    double           Anorm, error, work[1];
    double  c_neg_one = MAGMA_D_NEG_ONE;
    double *d, *e;
    double *hA, *hR, *tauq, *taup, *h_work;
    magma_int_t m, n, k;
    magma_int_t n2, lda, lwork, min_mn, nb, info;
    magma_int_t ione     = 1;
    int status = 0;
    magma_vect_t vect;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );

    double tol = opts.tolerance * lapackf77_dlamch("E");
    opts.lapack |= opts.check;  // check (-c) implies lapack (-l)
    
    magma_vect_t vects[] = { MagmaQ, MagmaP };
    
    printf("%% Q/P   m     n     k   CPU Gflop/s (sec)   GPU Gflop/s (sec)   ||R|| / ||A||\n");
    printf("%%============================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
      for( int ivect = 0; ivect < 2; ++ivect ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            m = opts.msize[itest];
            n = opts.nsize[itest];
            k = opts.ksize[itest];
            vect = vects[ivect];
            
            if ( (vect == MagmaQ && (m < n || n < min(m,k))) ||
                 (vect == MagmaP && (n < m || m < min(n,k))) )
            {
                printf( "%3c %5lld %5lld %5lld   skipping invalid dimensions\n",
                        lapacke_vect_const(vect), (long long) m, (long long) n, (long long) k );
                continue;
            }
            
            lda = m;
            n2 = lda*n;
            min_mn = min(m, n);
            nb = max( magma_get_dgelqf_nb( m, n ),
                      magma_get_dgebrd_nb( m, n ));
            lwork  = (m + n)*nb;
            
            if (vect == MagmaQ) {
                if (m >= k) {
                    gflops = FLOPS_DORGQR( m, n, k ) / 1e9;
                } else {
                    gflops = FLOPS_DORGQR( m-1, m-1, m-1 ) / 1e9;
                }
            }
            else {
                if (k < n) {
                    gflops = FLOPS_DORGLQ( m, n, k ) / 1e9;
                } else {
                    gflops = FLOPS_DORGLQ( n-1, n-1, n-1 ) / 1e9;
                }
            }
            
            TESTING_CHECK( magma_dmalloc_pinned( &h_work, lwork  ));
            TESTING_CHECK( magma_dmalloc_pinned( &hR,     lda*n  ));
            
            TESTING_CHECK( magma_dmalloc_cpu( &hA,     lda*n  ));
            TESTING_CHECK( magma_dmalloc_cpu( &tauq,   min_mn ));
            TESTING_CHECK( magma_dmalloc_cpu( &taup,   min_mn ));
            TESTING_CHECK( magma_dmalloc_cpu( &d,      min_mn   ));
            TESTING_CHECK( magma_dmalloc_cpu( &e,      min_mn-1 ));
            
            magma_generate_matrix( opts, m, n, hA, lda );
            lapackf77_dlacpy( MagmaFullStr, &m, &n, hA, &lda, hR, &lda );
            
            Anorm = lapackf77_dlange("f", &m, &n, hA, &lda, work );
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            // first, get GEBRD factors in both hA and hR
            magma_dgebrd( m, n, hA, lda, d, e, tauq, taup, h_work, lwork, &info );
            if (info != 0) {
                printf("magma_dgelqf_gpu returned error %lld: %s.\n",
                       (long long) info, magma_strerror( info ));
            }
            lapackf77_dlacpy( MagmaFullStr, &m, &n, hA, &lda, hR, &lda );
            
            gpu_time = magma_wtime();
            if (vect == MagmaQ) {
                magma_dorgbr( vect, m, n, k, hR, lda, tauq, h_work, lwork, &info );
            }
            else {
                magma_dorgbr( vect, m, n, k, hR, lda, taup, h_work, lwork, &info );
            }
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0) {
                printf("magma_dorgbr returned error %lld: %s.\n",
                       (long long) info, magma_strerror( info ));
            }
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                if (vect == MagmaQ) {
                    lapackf77_dorgbr( lapack_vect_const(vect), &m, &n, &k,
                                      hA, &lda, tauq, h_work, &lwork, &info );
                }
                else {
                    lapackf77_dorgbr( lapack_vect_const(vect), &m, &n, &k,
                                      hA, &lda, taup, h_work, &lwork, &info );
                }
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0) {
                    printf("lapackf77_dorgbr returned error %lld: %s.\n",
                           (long long) info, magma_strerror( info ));
                }
                
                if ( opts.verbose ) {
                    printf( "R=" );  magma_dprint( m, n, hR, lda );
                    printf( "A=" );  magma_dprint( m, n, hA, lda );
                }
                
                // compute relative error |R|/|A| := |Q_magma - Q_lapack|/|A|
                blasf77_daxpy( &n2, &c_neg_one, hA, &ione, hR, &ione );
                error = lapackf77_dlange("f", &m, &n, hR, &lda, work) / Anorm;
                
                if ( opts.verbose ) {
                    printf( "diff=" );  magma_dprint( m, n, hR, lda );
                }
                
                bool okay = (error < tol);
                status += ! okay;
                printf("%3c %5lld %5lld %5lld   %7.1f (%7.2f)   %7.1f (%7.2f)   %8.2e   %s\n",
                       lapacke_vect_const(vect), (long long) m, (long long) n, (long long) k,
                       cpu_perf, cpu_time, gpu_perf, gpu_time,
                       error, (okay ? "ok" : "failed"));
            }
            else {
                printf("%3c %5lld %5lld %5lld     ---   (  ---  )   %7.1f (%7.2f)     ---  \n",
                       lapacke_vect_const(vect), (long long) m, (long long) n, (long long) k,
                       gpu_perf, gpu_time );
            }
            
            magma_free_pinned( h_work );
            magma_free_pinned( hR     );
            
            magma_free_cpu( hA   );
            magma_free_cpu( tauq );
            magma_free_cpu( taup );
            magma_free_cpu( d );
            magma_free_cpu( e );
            fflush( stdout );
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
      }
    }
    
    opts.cleanup();
    TESTING_CHECK( magma_finalize() );
    return status;
}
