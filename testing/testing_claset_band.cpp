/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zlaset_band.cpp, normal z -> c, Sat Mar 27 20:31:54 2021
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
   -- Testing claset_band
   Code is very similar to testing_clacpy.cpp
*/
int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();
    
    #define h_A(i_,j_) (h_A + (i_) + (j_)*lda)
    #define d_A(i_,j_) (d_A + (i_) + (j_)*ldda)

    // Constants
    magmaFloatComplex  c_neg_one = MAGMA_C_NEG_ONE;
    magma_int_t ione     = 1;

    // Local variables
    real_Double_t    gbytes, gpu_perf, gpu_time, cpu_perf, cpu_time;
    float           error, work[1];
    magmaFloatComplex *h_A, *h_R;
    magmaFloatComplex_ptr d_A;
    magmaFloatComplex offdiag = MAGMA_C_MAKE( 1.2000, 6.7000 );
    magmaFloatComplex diag    = MAGMA_C_MAKE( 3.1415, 2.7183 );
    magma_int_t i, j, k, M, N, nb, cnt, size, lda, ldda;
    int status = 0;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );
    
    nb = (opts.nb == 0 ? 32 : opts.nb);

    magma_uplo_t uplo[] = { MagmaLower, MagmaUpper, MagmaFull };
    
    printf("%% K = nb = %lld\n", (long long) nb );
    printf("%% uplo      M     N   CPU GByte/s (ms)    GPU GByte/s (ms)    check\n");
    printf("%%=================================================================\n");
    for( int iuplo = 0; iuplo < 2; ++iuplo ) {
      for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            magma_int_t inset = 0;
            M = opts.msize[itest] + 2*inset;
            N = opts.nsize[itest] + 2*inset;
            lda    = M;
            ldda   = magma_roundup( M, opts.align );  // multiple of 32 by default
            size   = lda*N;
            
            TESTING_CHECK( magma_cmalloc_cpu( &h_A, size   ));
            TESTING_CHECK( magma_cmalloc_cpu( &h_R, size   ));
            
            TESTING_CHECK( magma_cmalloc( &d_A, ldda*N ));
            
            /* Initialize the matrix */
            for( j = 0; j < N; ++j ) {
                for( i = 0; i < M; ++i ) {
                    h_A[i + j*lda] = MAGMA_C_MAKE( i + j/10000., j );
                }
            }
            magma_csetmatrix( M, N, h_A, lda, d_A, ldda, opts.queue );
            
            /* =====================================================================
               Performs operation on CPU
               Also count number of elements touched.
               =================================================================== */
            cpu_time = magma_wtime();
            
            cnt = 0;
            for( j=inset; j < N-inset; ++j ) {
                for( k=0; k < nb; ++k ) {  // set k-th sub- or super-diagonal
                    if ( k == 0 && j < M-inset ) {
                        *h_A(j,j)   = diag;
                        cnt += 1;
                    }
                    else if ( uplo[iuplo] == MagmaLower && j+k < M-inset ) {
                        *h_A(j+k,j) = offdiag;
                        cnt += 1;
                    }
                    else if ( uplo[iuplo] == MagmaUpper && j-k >= inset && j-k < M-inset ) {
                        *h_A(j-k,j) = offdiag;
                        cnt += 1;
                    }
                }
            }
            
            gbytes = cnt / 1e9;
            
            cpu_time = magma_wtime() - cpu_time;
            cpu_perf = gbytes / cpu_time;
            
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            gpu_time = magma_sync_wtime( opts.queue );
            
            magma_int_t mm = M - 2*inset;
            magma_int_t nn = N - 2*inset;
            magmablas_claset_band( uplo[iuplo], mm, nn, nb, offdiag, diag, d_A(inset,inset), ldda, opts.queue );
            
            gpu_time = magma_sync_wtime( opts.queue ) - gpu_time;
            gpu_perf = gbytes / gpu_time;
            
            /* =====================================================================
               Check the result
               =================================================================== */
            magma_cgetmatrix( M, N, d_A, ldda, h_R, lda, opts.queue );
            
            //printf( "h_R=" );  magma_cprint( M, N, h_R, lda );
            //printf( "h_A=" );  magma_cprint( M, N, h_A, lda );

            blasf77_caxpy(&size, &c_neg_one, h_A, &ione, h_R, &ione);
            error = lapackf77_clange("f", &M, &N, h_R, &lda, work);
            
            printf("%4c   %5lld %5lld   %7.2f (%7.2f)   %7.2f (%7.2f)   %s\n",
                   lapacke_uplo_const( uplo[iuplo] ), (long long) M, (long long) N,
                   cpu_perf, cpu_time*1000., gpu_perf, gpu_time*1000.,
                   (error == 0. ? "ok" : "failed") );
            status += ! (error == 0.);
            
            magma_free_cpu( h_A );
            magma_free_cpu( h_R );
            
            magma_free( d_A );
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
