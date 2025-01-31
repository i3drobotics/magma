/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zswap.cpp, normal z -> s, Sat Mar 27 20:31:56 2021
       @author Mark Gates
*/
// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// includes, project
#include "magma_v2.h"
#include "magma_lapack.h"
#include "testings.h"


// if ( A == B ) return 0, else return 1
static magma_int_t diff_matrix(
    magma_int_t m, magma_int_t n,
    float *A, magma_int_t lda,
    float *B, magma_int_t ldb )
{
    for( magma_int_t j = 0; j < n; j++ ) {
        for( magma_int_t i = 0; i < m; i++ ) {
            if ( ! MAGMA_S_EQUAL( A[lda*j+i], B[ldb*j+i] ) )
                return 1;
        }
    }
    return 0;
}

// fill matrix with entries Aij = offset + (i+1) + (j+1)/10000,
// which makes it easy to identify which rows & cols have been swapped.
static void init_matrix(
    magma_int_t m, magma_int_t n,
    float *A, magma_int_t lda, magma_int_t offset )
{
    assert( lda >= m );
    for( magma_int_t j = 0; j < n; ++j ) {
        for( magma_int_t i=0; i < m; ++i ) {
            A[i + j*lda] = MAGMA_S_MAKE( offset + (i+1) + (j+1)/10000., 0 );
        }
    }
}

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing sswap, sswapblk, slaswp, slaswpx
*/
int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();
    
    // OpenCL use:  cl_mem  , offset  (two arguments);
    // else   use:  pointer + offset  (one argument).
    #ifdef HAVE_clBLAS
        #define d_A1(i_, j_)   d_A1    , (i_) + (j_)*ldda
        #define d_A2(i_, j_)   d_A2    , (i_) + (j_)*ldda
        #define d_ipiv(i_)     d_ipiv  , (i_)
    #else
        #define d_A1(i_, j_)  (d_A1    + (i_) + (j_)*ldda)
        #define d_A2(i_, j_)  (d_A2    + (i_) + (j_)*ldda)
        #define d_ipiv(i_)    (d_ipiv  + (i_))
    #endif
    
    #define h_A1(i_, j_)  (h_A1 + (i_) + (j_)*lda)
    #define h_A2(i_, j_)  (h_A2 + (i_) + (j_)*lda)

    float *h_A1, *h_A2;
    float *h_R1, *h_R2;
    magmaFloat_ptr d_A1, d_A2;
    
    // row-major and column-major performance
    real_Double_t row_perf0 = MAGMA_D_NAN, col_perf0 = MAGMA_D_NAN;
    real_Double_t row_perf1 = MAGMA_D_NAN, col_perf1 = MAGMA_D_NAN;
    real_Double_t row_perf2 = MAGMA_D_NAN, col_perf2 = MAGMA_D_NAN;
    real_Double_t row_perf4 = MAGMA_D_NAN;
    real_Double_t row_perf5 = MAGMA_D_NAN, col_perf5 = MAGMA_D_NAN;
    real_Double_t row_perf6 = MAGMA_D_NAN, col_perf6 = MAGMA_D_NAN;
    real_Double_t row_perf7 = MAGMA_D_NAN;
    real_Double_t cpu_perf  = MAGMA_D_NAN;

    real_Double_t time, gbytes;

    magma_int_t N, lda, ldda, nb, j;
    magma_int_t ione = 1;
    magma_int_t *ipiv, *ipiv2;
    magmaInt_ptr d_ipiv;
    int status = 0;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );

    printf("%%           %8s sswap    sswap             sswapblk          slaswp   slaswp2  slaswpx           scopymatrix      CPU      (all in )\n", g_platform_str );
    printf("%%   N   nb  row-maj/col-maj   row-maj/col-maj   row-maj/col-maj   row-maj  row-maj  row-maj/col-maj   row-blk/col-blk  slaswp   (GByte/s)\n");
    printf("%%========================================================================================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            // For an N x N matrix, swap nb rows or nb columns using various methods.
            // Each test is assigned one bit in the 'check' bitmask; bit=1 indicates failure.
            // The variable 'shift' keeps track of which bit is for current test
            magma_int_t shift = 1;
            magma_int_t check = 0;
            N = opts.nsize[itest];
            lda    = N;
            ldda   = magma_roundup( N, opts.align );  // multiple of 32 by default
            nb     = (opts.nb > 0 ? opts.nb : magma_get_sgetrf_nb( N, N ));
            nb     = min( N, nb );
            // each swap does 2N loads and 2N stores, for nb swaps
            gbytes = sizeof(float) * 4.*N*nb / 1e9;
            
            TESTING_CHECK( magma_smalloc_pinned( &h_A1, lda*N ));
            TESTING_CHECK( magma_smalloc_pinned( &h_A2, lda*N ));
            TESTING_CHECK( magma_smalloc_pinned( &h_R1, lda*N ));
            TESTING_CHECK( magma_smalloc_pinned( &h_R2, lda*N ));
            
            TESTING_CHECK( magma_imalloc_cpu( &ipiv,  nb ));
            TESTING_CHECK( magma_imalloc_cpu( &ipiv2, nb ));
            
            TESTING_CHECK( magma_imalloc( &d_ipiv, nb ));
            TESTING_CHECK( magma_smalloc( &d_A1, ldda*N ));
            TESTING_CHECK( magma_smalloc( &d_A2, ldda*N ));
            
            // getrf always makes ipiv[j] >= j+1, where ipiv is one based and j is zero based
            // some implementations (e.g., MacOS dlaswp) assume this
            for( j=0; j < nb; j++ ) {
                ipiv[j] = (rand() % (N-j)) + j + 1;
                assert( ipiv[j] >= j+1 );
                assert( ipiv[j] <= N   );
            }
            
            /* =====================================================================
             * cublas / clBLAS / Xeon Phi sswap, row-by-row (2 matrices)
             */
            
            /* Row Major */
            init_matrix( N, N, h_A1, lda, 0 );
            init_matrix( N, N, h_A2, lda, 100 );
            magma_ssetmatrix( N, N, h_A1, lda, d_A1(0,0), ldda, opts.queue );
            magma_ssetmatrix( N, N, h_A2, lda, d_A2(0,0), ldda, opts.queue );
            
            time = magma_sync_wtime( opts.queue );
            for( j=0; j < nb; j++) {
                if ( j != (ipiv[j]-1)) {
                    magma_sswap( N, d_A1(0,j), 1, d_A2(0,ipiv[j]-1), 1, opts.queue );
                }
            }
            time = magma_sync_wtime( opts.queue ) - time;
            row_perf0 = gbytes / time;
            
            for( j=0; j < nb; j++) {
                if ( j != (ipiv[j]-1)) {
                    blasf77_sswap( &N, h_A1(0,j), &ione, h_A2(0,ipiv[j]-1), &ione);
                }
            }
            magma_sgetmatrix( N, N, d_A1(0,0), ldda, h_R1, lda, opts.queue );
            magma_sgetmatrix( N, N, d_A2(0,0), ldda, h_R2, lda, opts.queue );
            check += (diff_matrix( N, N, h_A1, lda, h_R1, lda ) ||
                      diff_matrix( N, N, h_A2, lda, h_R2, lda ))*shift;
            shift *= 2;
            
            /* Column Major */
            init_matrix( N, N, h_A1, lda, 0 );
            init_matrix( N, N, h_A2, lda, 100 );
            magma_ssetmatrix( N, N, h_A1, lda, d_A1(0,0), ldda, opts.queue );
            magma_ssetmatrix( N, N, h_A2, lda, d_A2(0,0), ldda, opts.queue );
            
            time = magma_sync_wtime( opts.queue );
            for( j=0; j < nb; j++) {
                if ( j != (ipiv[j]-1)) {
                    magma_sswap( N, d_A1(j,0), ldda, d_A2(ipiv[j]-1,0), ldda, opts.queue );
                }
            }
            time = magma_sync_wtime( opts.queue ) - time;
            col_perf0 = gbytes / time;
            
            for( j=0; j < nb; j++) {
                if ( j != (ipiv[j]-1)) {
                    blasf77_sswap( &N, h_A1+j, &lda, h_A2+(ipiv[j]-1), &lda);
                }
            }
            magma_sgetmatrix( N, N, d_A1(0,0), ldda, h_R1, lda, opts.queue );
            magma_sgetmatrix( N, N, d_A2(0,0), ldda, h_R2, lda, opts.queue );
            check += (diff_matrix( N, N, h_A1, lda, h_R1, lda ) ||
                      diff_matrix( N, N, h_A2, lda, h_R2, lda ))*shift;
            shift *= 2;

            /* =====================================================================
             * sswap, row-by-row (2 matrices)
             */
            
            /* Row Major */
            init_matrix( N, N, h_A1, lda, 0 );
            init_matrix( N, N, h_A2, lda, 100 );
            magma_ssetmatrix( N, N, h_A1, lda, d_A1(0,0), ldda, opts.queue );
            magma_ssetmatrix( N, N, h_A2, lda, d_A2(0,0), ldda, opts.queue );
            
            time = magma_sync_wtime( opts.queue );
            for( j=0; j < nb; j++) {
                if ( j != (ipiv[j]-1)) {
                    magmablas_sswap( N, d_A1(0,j), 1, d_A2(0,ipiv[j]-1), 1, opts.queue );
                }
            }
            time = magma_sync_wtime( opts.queue ) - time;
            row_perf1 = gbytes / time;
            
            for( j=0; j < nb; j++) {
                if ( j != (ipiv[j]-1)) {
                    blasf77_sswap( &N, h_A1(0,j), &ione, h_A2(0,ipiv[j]-1), &ione);
                }
            }
            magma_sgetmatrix( N, N, d_A1(0,0), ldda, h_R1, lda, opts.queue );
            magma_sgetmatrix( N, N, d_A2(0,0), ldda, h_R2, lda, opts.queue );
            check += (diff_matrix( N, N, h_A1, lda, h_R1, lda ) ||
                      diff_matrix( N, N, h_A2, lda, h_R2, lda ))*shift;
            shift *= 2;
            
            /* Column Major */
            init_matrix( N, N, h_A1, lda, 0 );
            init_matrix( N, N, h_A2, lda, 100 );
            magma_ssetmatrix( N, N, h_A1, lda, d_A1(0,0), ldda, opts.queue );
            magma_ssetmatrix( N, N, h_A2, lda, d_A2(0,0), ldda, opts.queue );
            
            time = magma_sync_wtime( opts.queue );
            for( j=0; j < nb; j++) {
                if ( j != (ipiv[j]-1)) {
                    magmablas_sswap( N, d_A1(j,0), ldda, d_A2(ipiv[j]-1,0), ldda, opts.queue );
                }
            }
            time = magma_sync_wtime( opts.queue ) - time;
            col_perf1 = gbytes / time;
            
            for( j=0; j < nb; j++) {
                if ( j != (ipiv[j]-1)) {
                    blasf77_sswap( &N, h_A1+j, &lda, h_A2+(ipiv[j]-1), &lda);
                }
            }
            magma_sgetmatrix( N, N, d_A1(0,0), ldda, h_R1, lda, opts.queue );
            magma_sgetmatrix( N, N, d_A2(0,0), ldda, h_R2, lda, opts.queue );
            check += (diff_matrix( N, N, h_A1, lda, h_R1, lda ) ||
                      diff_matrix( N, N, h_A2, lda, h_R2, lda ))*shift;
            shift *= 2;

            /* =====================================================================
             * sswapblk, blocked version (2 matrices)
             */
            
            #if defined(HAVE_CUBLAS) || defined(HAVE_HIP)
                /* Row Major */
                init_matrix( N, N, h_A1, lda, 0 );
                init_matrix( N, N, h_A2, lda, 100 );
                magma_ssetmatrix( N, N, h_A1, lda, d_A1(0,0), ldda, opts.queue );
                magma_ssetmatrix( N, N, h_A2, lda, d_A2(0,0), ldda, opts.queue );
                
                time = magma_sync_wtime( opts.queue );
                magmablas_sswapblk( MagmaRowMajor, N, d_A1(0,0), ldda, d_A2(0,0), ldda, 1, nb, ipiv, 1, 0, opts.queue );
                time = magma_sync_wtime( opts.queue ) - time;
                row_perf2 = gbytes / time;
                
                for( j=0; j < nb; j++) {
                    if ( j != (ipiv[j]-1)) {
                        blasf77_sswap( &N, h_A1(0,j), &ione, h_A2(0,ipiv[j]-1), &ione );
                    }
                }
                magma_sgetmatrix( N, N, d_A1(0,0), ldda, h_R1, lda, opts.queue );
                magma_sgetmatrix( N, N, d_A2(0,0), ldda, h_R2, lda, opts.queue );
                check += (diff_matrix( N, N, h_A1, lda, h_R1, lda ) ||
                          diff_matrix( N, N, h_A2, lda, h_R2, lda ))*shift;
                shift *= 2;
                
                /* Column Major */
                init_matrix( N, N, h_A1, lda, 0 );
                init_matrix( N, N, h_A2, lda, 100 );
                magma_ssetmatrix( N, N, h_A1, lda, d_A1(0,0), ldda, opts.queue );
                magma_ssetmatrix( N, N, h_A2, lda, d_A2(0,0), ldda, opts.queue );
                
                time = magma_sync_wtime( opts.queue );
                magmablas_sswapblk( MagmaColMajor, N, d_A1(0,0), ldda, d_A2(0,0), ldda, 1, nb, ipiv, 1, 0, opts.queue );
                time = magma_sync_wtime( opts.queue ) - time;
                col_perf2 = gbytes / time;
                
                for( j=0; j < nb; j++) {
                    if ( j != (ipiv[j]-1)) {
                        blasf77_sswap( &N, h_A1(j,0), &lda, h_A2(ipiv[j]-1,0), &lda );
                    }
                }
                magma_sgetmatrix( N, N, d_A1(0,0), ldda, h_R1, lda, opts.queue );
                magma_sgetmatrix( N, N, d_A2(0,0), ldda, h_R2, lda, opts.queue );
                check += (diff_matrix( N, N, h_A1, lda, h_R1, lda ) ||
                          diff_matrix( N, N, h_A2, lda, h_R2, lda ))*shift;
                shift *= 2;
            #endif
            
            /* =====================================================================
             * LAPACK-style slaswp (1 matrix)
             */
            
            /* Row Major */
            init_matrix( N, N, h_A1, lda, 0 );
            magma_ssetmatrix( N, N, h_A1, lda, d_A1(0,0), ldda, opts.queue );
            
            time = magma_sync_wtime( opts.queue );
            magmablas_slaswp( N, d_A1(0,0), ldda, 1, nb, ipiv, 1, opts.queue );
            time = magma_sync_wtime( opts.queue ) - time;
            row_perf4 = gbytes / time;
            
            for( j=0; j < nb; j++) {
                if ( j != (ipiv[j]-1)) {
                    blasf77_sswap( &N, h_A1(0,j), &ione, h_A1(0,ipiv[j]-1), &ione );
                }
            }
            magma_sgetmatrix( N, N, d_A1(0,0), ldda, h_R1, lda, opts.queue );
            check += diff_matrix( N, N, h_A1, lda, h_R1, lda )*shift;
            shift *= 2;

            /* =====================================================================
             * LAPACK-style slaswp (1 matrix) - d_ipiv on GPU
             */
            
            /* Row Major */
            init_matrix( N, N, h_A1, lda, 0 );
            magma_ssetmatrix( N, N, h_A1, lda, d_A1(0,0), ldda, opts.queue );
            
            time = magma_sync_wtime( opts.queue );
            magma_isetvector( nb, ipiv, 1, d_ipiv(0), 1, opts.queue );
            magmablas_slaswp2( N, d_A1(0,0), ldda, 1, nb, d_ipiv(0), 1, opts.queue );
            time = magma_sync_wtime( opts.queue ) - time;
            row_perf7 = gbytes / time;
            
            for( j=0; j < nb; j++) {
                if ( j != (ipiv[j]-1)) {
                    blasf77_sswap( &N, h_A1(0,j), &ione, h_A1(0,ipiv[j]-1), &ione );
                }
            }
            magma_sgetmatrix( N, N, d_A1(0,0), ldda, h_R1, lda, opts.queue );
            check += diff_matrix( N, N, h_A1, lda, h_R1, lda )*shift;
            shift *= 2;

            /* =====================================================================
             * LAPACK-style slaswpx (extended for row- and col-major) (1 matrix)
             */
            
            /* Row Major */
            init_matrix( N, N, h_A1, lda, 0 );
            magma_ssetmatrix( N, N, h_A1, lda, d_A1(0,0), ldda, opts.queue );
            
            time = magma_sync_wtime( opts.queue );
            magmablas_slaswpx( N, d_A1(0,0), ldda, 1, 1, nb, ipiv, 1, opts.queue );
            time = magma_sync_wtime( opts.queue ) - time;
            row_perf5 = gbytes / time;
            
            for( j=0; j < nb; j++) {
                if ( j != (ipiv[j]-1)) {
                    blasf77_sswap( &N, h_A1(0,j), &ione, h_A1(0,ipiv[j]-1), &ione );
                }
            }
            magma_sgetmatrix( N, N, d_A1(0,0), ldda, h_R1, lda, opts.queue );
            check += diff_matrix( N, N, h_A1, lda, h_R1, lda )*shift;
            shift *= 2;
            
            /* Col Major */
            init_matrix( N, N, h_A1, lda, 0 );
            magma_ssetmatrix( N, N, h_A1, lda, d_A1(0,0), ldda, opts.queue );
            
            time = magma_sync_wtime( opts.queue );
            magmablas_slaswpx( N, d_A1(0,0), 1, ldda, 1, nb, ipiv, 1, opts.queue );
            time = magma_sync_wtime( opts.queue ) - time;
            col_perf5 = gbytes / time;
            
            /* LAPACK swap on CPU for comparison */
            time = magma_wtime();
            lapackf77_slaswp( &N, h_A1, &lda, &ione, &nb, ipiv, &ione );
            time = magma_wtime() - time;
            cpu_perf = gbytes / time;
            
            magma_sgetmatrix( N, N, d_A1(0,0), ldda, h_R1, lda, opts.queue );
            check += diff_matrix( N, N, h_A1, lda, h_R1, lda )*shift;
            shift *= 2;

            /* =====================================================================
             * Copy matrix.
             */
            
            time = magma_sync_wtime( opts.queue );
            magma_scopymatrix( N, nb, d_A1(0,0), ldda, d_A2(0,0), ldda, opts.queue );
            time = magma_sync_wtime( opts.queue ) - time;
            // copy reads 1 matrix and writes 1 matrix, so has half gbytes of swap
            col_perf6 = 0.5 * gbytes / time;
            
            time = magma_sync_wtime( opts.queue );
            magma_scopymatrix( nb, N, d_A1(0,0), ldda, d_A2(0,0), ldda, opts.queue );
            time = magma_sync_wtime( opts.queue ) - time;
            // copy reads 1 matrix and writes 1 matrix, so has half gbytes of swap
            row_perf6 = 0.5 * gbytes / time;

            printf("%5lld  %3lld  %6.2f%c/ %6.2f%c  %6.2f%c/ %6.2f%c  %6.2f%c/ %6.2f%c  %6.2f%c  %6.2f%c  %6.2f%c/ %6.2f%c  %6.2f / %6.2f  %6.2f  %10s\n",
                   (long long) N, (long long) nb,
                   row_perf0, ((check & 0x001) != 0 ? '*' : ' '),
                   col_perf0, ((check & 0x002) != 0 ? '*' : ' '),
                   row_perf1, ((check & 0x004) != 0 ? '*' : ' '),
                   col_perf1, ((check & 0x008) != 0 ? '*' : ' '),
                   row_perf2, ((check & 0x010) != 0 ? '*' : ' '),
                   col_perf2, ((check & 0x020) != 0 ? '*' : ' '),
                   row_perf4, ((check & 0x040) != 0 ? '*' : ' '),
                   row_perf7, ((check & 0x080) != 0 ? '*' : ' '),
                   row_perf5, ((check & 0x100) != 0 ? '*' : ' '),
                   col_perf5, ((check & 0x200) != 0 ? '*' : ' '),
                   row_perf6,
                   col_perf6,
                   cpu_perf,
                   (check == 0 ? "ok" : "* failed") );
            status += ! (check == 0);
            
            magma_free_pinned( h_A1 );
            magma_free_pinned( h_A2 );
            magma_free_pinned( h_R1 );
            magma_free_pinned( h_R2 );
            
            magma_free_cpu( ipiv  );
            magma_free_cpu( ipiv2 );
            
            magma_free( d_ipiv );
            magma_free( d_A1 );
            magma_free( d_A2 );
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
