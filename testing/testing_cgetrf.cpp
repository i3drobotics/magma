/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zgetrf.cpp, normal z -> c, Sat Mar 27 20:32:03 2021
       @author Mark Gates
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


// Initialize matrix to random.
// This ensures the same ISEED is always used,
// so we can re-generate the identical matrix.
void init_matrix(
    magma_opts &opts,
    magma_int_t m, magma_int_t n,
    magmaFloatComplex *A, magma_int_t lda )
{
    magma_int_t iseed_save[4];
    for (magma_int_t i = 0; i < 4; ++i) {
        iseed_save[i] = opts.iseed[i];
    }

    magma_generate_matrix( opts, m, n, A, lda );

    // restore iseed
    for (magma_int_t i = 0; i < 4; ++i) {
        opts.iseed[i] = iseed_save[i];
    }
}


// On input, A and ipiv is LU factorization of A. On output, A is overwritten.
// Requires m == n.
// Uses init_matrix() to re-generate original A as needed.
// Generates random RHS b and solves Ax=b.
// Returns residual, |Ax - b| / (n |A| |x|).
float get_residual(
    magma_opts &opts,
    magma_int_t m, magma_int_t n,
    magmaFloatComplex *A, magma_int_t lda,
    magma_int_t *ipiv )
{
    if ( m != n ) {
        printf( "\nERROR: residual check defined only for square matrices\n" );
        return -1;
    }
    
    const magmaFloatComplex c_one     = MAGMA_C_ONE;
    const magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    const magma_int_t ione = 1;
    
    // this seed should be DIFFERENT than used in init_matrix
    // (else x is column of A, so residual can be exactly zero)
    magma_int_t ISEED[4] = {0,0,0,1};
    magma_int_t info = 0;
    magmaFloatComplex *x, *b;
    
    // initialize RHS
    TESTING_CHECK( magma_cmalloc_cpu( &x, n ));
    TESTING_CHECK( magma_cmalloc_cpu( &b, n ));
    lapackf77_clarnv( &ione, ISEED, &n, b );
    blasf77_ccopy( &n, b, &ione, x, &ione );
    
    // solve Ax = b
    lapackf77_cgetrs( "Notrans", &n, &ione, A, &lda, ipiv, x, &n, &info );
    if (info != 0) {
        printf("lapackf77_cgetrs returned error %lld: %s.\n",
               (long long) info, magma_strerror( info ));
    }
    
    // reset to original A
    init_matrix( opts, m, n, A, lda );
    
    // compute r = Ax - b, saved in b
    blasf77_cgemv( "Notrans", &m, &n, &c_one, A, &lda, x, &ione, &c_neg_one, b, &ione );
    
    // compute residual |Ax - b| / (n*|A|*|x|)
    float norm_x, norm_A, norm_r, work[1];
    norm_A = lapackf77_clange( "F", &m, &n, A, &lda, work );
    norm_r = lapackf77_clange( "F", &n, &ione, b, &n, work );
    norm_x = lapackf77_clange( "F", &n, &ione, x, &n, work );
    
    //printf( "r=\n" ); magma_cprint( 1, n, b, 1 );
    
    magma_free_cpu( x );
    magma_free_cpu( b );
    
    //printf( "r=%.2e, A=%.2e, x=%.2e, n=%lld\n", norm_r, norm_A, norm_x, (long long) n );
    return norm_r / (n * norm_A * norm_x);
}


// On input, LU and ipiv is LU factorization of A. On output, LU is overwritten.
// Works for any m, n.
// Uses init_matrix() to re-generate original A as needed.
// Returns error in factorization, |PA - LU| / (n |A|)
// This allocates 3 more matrices to store A, L, and U.
float get_LU_error(
    magma_opts &opts,
    magma_int_t M, magma_int_t N,
    magmaFloatComplex *LU, magma_int_t lda,
    magma_int_t *ipiv)
{
    magma_int_t min_mn = min(M,N);
    magma_int_t ione   = 1;
    magma_int_t i, j;
    magmaFloatComplex alpha = MAGMA_C_ONE;
    magmaFloatComplex beta  = MAGMA_C_ZERO;
    magmaFloatComplex *A, *L, *U;
    float work[1], matnorm, residual;
    
    TESTING_CHECK( magma_cmalloc_cpu( &A, lda*N    ));
    TESTING_CHECK( magma_cmalloc_cpu( &L, M*min_mn ));
    TESTING_CHECK( magma_cmalloc_cpu( &U, min_mn*N ));
    memset( L, 0, M*min_mn*sizeof(magmaFloatComplex) );
    memset( U, 0, min_mn*N*sizeof(magmaFloatComplex) );

    // set to original A
    init_matrix( opts, M, N, A, lda );
    lapackf77_claswp( &N, A, &lda, &ione, &min_mn, ipiv, &ione);
    
    // copy LU to L and U, and set diagonal to 1
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

    magma_free_cpu( A );
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
    magmaFloatComplex *h_A;
    magma_int_t     *ipiv;
    magma_int_t     M, N, n2, lda, info, min_mn;
    int status = 0;
    
    magma_opts opts;
    opts.parse_opts( argc, argv );
    
    float tol = opts.tolerance * lapackf77_slamch("E");

    printf("%% ngpu %lld, version %lld\n", (long long) opts.ngpu, (long long) opts.version );
    if ( opts.check == 2 ) {
        printf("%%   M     N   CPU Gflop/s (sec)   GPU Gflop/s (sec)   |Ax-b|/(N*|A|*|x|)\n");
    }
    else {
        printf("%%   M     N   CPU Gflop/s (sec)   GPU Gflop/s (sec)   |PA-LU|/(N*|A|)\n");
    }
    printf("%%========================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M = opts.msize[itest];
            N = opts.nsize[itest];
            min_mn = min(M, N);
            lda    = M;
            n2     = lda*N;
            gflops = FLOPS_CGETRF( M, N ) / 1e9;
            
            TESTING_CHECK( magma_imalloc_cpu( &ipiv, min_mn ));
            TESTING_CHECK( magma_cmalloc_pinned( &h_A,  n2 ));
            
            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                init_matrix( opts, M, N, h_A, lda );
                
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
            init_matrix( opts, M, N, h_A, lda );
            if ( opts.version == 2 || opts.version == 3 ) {
                // no pivoting versions, so set ipiv to identity
                for (magma_int_t i=0; i < min_mn; ++i ) {
                    ipiv[i] = i+1;
                }
            }
            
            gpu_time = magma_wtime();
            if ( opts.version == 1 ) {
                magma_cgetrf( M, N, h_A, lda, ipiv, &info );
            }
            else if ( opts.version == 2 ) {
                magma_cgetrf_nopiv( M, N, h_A, lda, &info );
            }
            else if ( opts.version == 3 ) {
                magma_cgetf2_nopiv( M, N, h_A, lda, &info );
            }
            gpu_time = magma_wtime() - gpu_time;
            gpu_perf = gflops / gpu_time;
            if (info != 0) {
                printf("magma_cgetrf returned error %lld: %s.\n",
                       (long long) info, magma_strerror( info ));
            }
            
            /* =====================================================================
               Check the factorization
               =================================================================== */
            if ( opts.lapack ) {
                printf("%5lld %5lld   %7.2f (%7.2f)   %7.2f (%7.2f)",
                       (long long) M, (long long) N, cpu_perf, cpu_time, gpu_perf, gpu_time );
            }
            else {
                printf("%5lld %5lld     ---   (  ---  )   %7.2f (%7.2f)",
                       (long long) M, (long long) N, gpu_perf, gpu_time );
            }
            if ( opts.check == 2 ) {
                error = get_residual( opts, M, N, h_A, lda, ipiv );
                printf("   %8.2e   %s\n", error, (error < tol ? "ok" : "failed"));
                status += ! (error < tol);
            }
            else if ( opts.check ) {
                error = get_LU_error( opts, M, N, h_A, lda, ipiv );
                printf("   %8.2e   %s\n", error, (error < tol ? "ok" : "failed"));
                status += ! (error < tol);
            }
            else {
                printf("     ---   \n");
            }
            
            magma_free_cpu( ipiv );
            magma_free_pinned( h_A  );
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
