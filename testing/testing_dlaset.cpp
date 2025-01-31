/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zlaset.cpp, normal z -> d, Sat Mar 27 20:31:54 2021
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
   -- Testing dlaset
   Code is very similar to testing_dlacpy.cpp
*/
int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    real_Double_t    gbytes, gpu_perf, gpu_time, cpu_perf, cpu_time;
    double           error, work[1];
    double  c_neg_one = MAGMA_D_NEG_ONE;
    double *h_A, *h_R;
    magmaDouble_ptr d_A;
    double offdiag, diag;
    magma_int_t M, N, size, lda, ldda;
    magma_int_t ione     = 1;
    int status = 0;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );

    magma_uplo_t uplo[] = { MagmaLower, MagmaUpper, MagmaFull };

    printf("%% uplo    M     N    offdiag    diag    CPU GByte/s (ms)    GPU GByte/s (ms)   check\n");
    printf("%%===================================================================================\n");
    for( int iuplo = 0; iuplo < 3; ++iuplo ) {
      for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
          for( int ival = 0; ival < 4; ++ival ) {
            // test combinations of zero & non-zero:
            // ival  offdiag  diag
            // 0     0        0
            // 1     0        3.14
            // 2     1.23     0
            // 3     1.23     3.14
            offdiag = MAGMA_D_MAKE( 1.2345, 6.7890 ) * (double)(ival / 2);
            diag    = MAGMA_D_MAKE( 3.1415, 2.7183 ) * (double)(ival % 2);
            
            M = opts.msize[itest];
            N = opts.nsize[itest];
            //M += 2;  // space for insets
            //N += 2;
            lda    = M;
            ldda   = magma_roundup( M, opts.align );
            size   = lda*N;
            if ( uplo[iuplo] == MagmaLower ) {
                // save lower trapezoid (with diagonal)
                if ( M > N ) {
                    gbytes = sizeof(double) * (1.*M*N - 0.5*N*(N-1)) / 1e9;
                } else {
                    gbytes = sizeof(double) * 0.5*M*(M+1) / 1e9;
                }
            }
            else if ( uplo[iuplo] == MagmaUpper ) {
                // save upper trapezoid (with diagonal)
                if ( N > M ) {
                    gbytes = sizeof(double) * (1.*M*N - 0.5*M*(M-1)) / 1e9;
                } else {
                    gbytes = sizeof(double) * 0.5*N*(N+1) / 1e9;
                }
            }
            else {
                // save entire matrix
                gbytes = sizeof(double) * 1.*M*N / 1e9;
            }
    
            TESTING_CHECK( magma_dmalloc_cpu( &h_A, size   ));
            TESTING_CHECK( magma_dmalloc_cpu( &h_R, size   ));
            
            TESTING_CHECK( magma_dmalloc( &d_A, ldda*N ));
            
            /* Initialize the matrix */
            for( int j = 0; j < N; ++j ) {
                for( int i = 0; i < M; ++i ) {
                    h_A[i + j*lda] = MAGMA_D_MAKE( i + j/10000., j );
                }
            }
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            magma_dsetmatrix( M, N, h_A, lda, d_A, ldda, opts.queue );
            
            gpu_time = magma_sync_wtime( opts.queue );
            //magmablas_dlaset( uplo[iuplo], M-2, N-2, offdiag, diag, d_A+1+ldda, ldda, opts.queue );  // inset by 1 row & col
            magmablas_dlaset( uplo[iuplo], M, N, offdiag, diag, d_A, ldda, opts.queue );
            gpu_time = magma_sync_wtime( opts.queue ) - gpu_time;
            gpu_perf = gbytes / gpu_time;
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            cpu_time = magma_wtime();
            //magma_int_t M2 = M-2;  // inset by 1 row & col
            //magma_int_t N2 = N-2;
            //lapackf77_dlaset( lapack_uplo_const( uplo[iuplo] ), &M2, &N2, &offdiag, &diag, h_A+1+lda, &lda );
            lapackf77_dlaset( lapack_uplo_const( uplo[iuplo] ), &M, &N, &offdiag, &diag, h_A, &lda );
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gbytes / cpu_time;
            
            if ( opts.verbose ) {
                printf( "A= " );  magma_dprint(     M, N, h_A, lda );
                printf( "dA=" );  magma_dprint_gpu( M, N, d_A, ldda, opts.queue );
            }
            
            /* =====================================================================
               Check the result
               =================================================================== */
            magma_dgetmatrix( M, N, d_A, ldda, h_R, lda, opts.queue );
            
            blasf77_daxpy(&size, &c_neg_one, h_A, &ione, h_R, &ione);
            error = lapackf77_dlange("f", &M, &N, h_R, &lda, work);

            bool okay = (error == 0);
            status += ! okay;
            printf("%5s %5lld %5lld  %9.4f  %6.4f   %7.2f (%7.2f)   %7.2f (%7.2f)   %s\n",
                   lapack_uplo_const( uplo[iuplo] ), (long long) M, (long long) N,
                   real(offdiag), real(diag),
                   cpu_perf, cpu_time*1000., gpu_perf, gpu_time*1000.,
                   (okay ? "ok" : "failed") );
            
            magma_free_cpu( h_A );
            magma_free_cpu( h_R );
            
            magma_free( d_A );
            fflush( stdout );
          }
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
      }
      printf( "\n" );
    }

    opts.cleanup();
    TESTING_CHECK( magma_finalize() );
    return status;
}
