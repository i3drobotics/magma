/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zlacpy.cpp, normal z -> c, Sat Mar 27 20:31:52 2021
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
   -- Testing clacpy
*/
int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    real_Double_t    gbytes, gpu_perf, gpu_time, cpu_perf, cpu_time;
    float           error, work[1];
    magmaFloatComplex  c_neg_one = MAGMA_C_NEG_ONE;
    magmaFloatComplex *h_A, *h_B, *h_R;
    magmaFloatComplex_ptr d_A, d_B;
    magma_int_t M, N, size, lda, ldb, ldda, lddb;
    magma_int_t ione     = 1;
    int status = 0;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );

    magma_uplo_t uplo[] = { MagmaLower, MagmaUpper, MagmaFull };
    
    printf("%% uplo    M     N   CPU GByte/s (ms)    GPU GByte/s (ms)    check\n");
    printf("%%================================================================\n");
    for( int iuplo = 0; iuplo < 3; ++iuplo ) {
      for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[itest];
            N = opts.nsize[itest];
            lda    = M;
            ldb    = lda;
            ldda   = magma_roundup( M, opts.align );  // multiple of 32 by default
            lddb   = ldda;
            size   = lda*N;
            if ( uplo[iuplo] == MagmaLower ) {
                // load & save lower trapezoid (with diagonal)
                if ( M > N ) {
                    gbytes = 2. * sizeof(magmaFloatComplex) * (1.*M*N - 0.5*N*(N-1)) / 1e9;
                } else {
                    gbytes = 2. * sizeof(magmaFloatComplex) * 0.5*M*(M+1) / 1e9;
                }
            }
            else if ( uplo[iuplo] == MagmaUpper ) {
                // load & save upper trapezoid (with diagonal)
                if ( N > M ) {
                    gbytes = 2. * sizeof(magmaFloatComplex) * (1.*M*N - 0.5*M*(M-1)) / 1e9;
                } else {
                    gbytes = 2. * sizeof(magmaFloatComplex) * 0.5*N*(N+1) / 1e9;
                }
            }
            else {
                // load & save entire matrix
                gbytes = 2. * sizeof(magmaFloatComplex) * 1.*M*N / 1e9;
            }
    
            TESTING_CHECK( magma_cmalloc_cpu( &h_A, size   ));
            TESTING_CHECK( magma_cmalloc_cpu( &h_B, size   ));
            TESTING_CHECK( magma_cmalloc_cpu( &h_R, size   ));
            
            TESTING_CHECK( magma_cmalloc( &d_A, ldda*N ));
            TESTING_CHECK( magma_cmalloc( &d_B, lddb*N ));
            
            /* Initialize the matrix */
            for( int j = 0; j < N; ++j ) {
                for( int i = 0; i < M; ++i ) {
                    h_A[i + j*lda] = MAGMA_C_MAKE( i + j/10000., j );
                    h_B[i + j*ldb] = MAGMA_C_MAKE( i - j/10000. + 10000., j );
                }
            }
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            magma_csetmatrix( M, N, h_A, lda, d_A, ldda, opts.queue );
            magma_csetmatrix( M, N, h_B, ldb, d_B, lddb, opts.queue );
            
            gpu_time = magma_sync_wtime( opts.queue );
            //magmablas_clacpy( uplo[iuplo], M-2, N-2, d_A+1+ldda, ldda, d_B+1+lddb, lddb, opts.queue );  // inset by 1 row & col
            magmablas_clacpy( uplo[iuplo], M, N, d_A, ldda, d_B, lddb, opts.queue );
            gpu_time = magma_sync_wtime( opts.queue ) - gpu_time;
            gpu_perf = gbytes / gpu_time;
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            cpu_time = magma_wtime();
            //magma_int_t M2 = M-2;  // inset by 1 row & col
            //magma_int_t N2 = N-2;
            //lapackf77_clacpy( lapack_uplo_const(uplo[iuplo]), &M2, &N2, h_A+1+lda, &lda, h_B+1+ldb, &ldb );
            lapackf77_clacpy( lapack_uplo_const(uplo[iuplo]), &M, &N, h_A, &lda, h_B, &ldb );
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gbytes / cpu_time;
            
            if ( opts.verbose ) {
                printf( "A= " );  magma_cprint(     M, N, h_A, lda );
                printf( "B= " );  magma_cprint(     M, N, h_B, ldb );
                printf( "dA=" );  magma_cprint_gpu( M, N, d_A, ldda, opts.queue );
                printf( "dB=" );  magma_cprint_gpu( M, N, d_B, lddb, opts.queue );
            }
            
            /* =====================================================================
               Check the result
               =================================================================== */
            magma_cgetmatrix( M, N, d_B, lddb, h_R, lda, opts.queue );
            
            blasf77_caxpy(&size, &c_neg_one, h_B, &ione, h_R, &ione);
            error = lapackf77_clange("f", &M, &N, h_R, &lda, work);

            printf("%5s %5lld %5lld   %7.2f (%7.2f)   %7.2f (%7.2f)   %s\n",
                   lapack_uplo_const(uplo[iuplo]), (long long) M, (long long) N,
                   cpu_perf, cpu_time*1000., gpu_perf, gpu_time*1000.,
                   (error == 0. ? "ok" : "failed") );
            status += ! (error == 0.);
            
            magma_free_cpu( h_A );
            magma_free_cpu( h_B );
            magma_free_cpu( h_R );
            
            magma_free( d_A );
            magma_free( d_B );
            fflush( stdout );
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
