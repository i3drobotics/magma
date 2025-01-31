/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from testing/testing_zher2k_batched.cpp, normal z -> s, Sat Mar 27 20:32:16 2021
       @author Mark Gates
       @author Azzam Haidar
       @author Tingxing Dong
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

#if defined(_OPENMP)
#include <omp.h>
#endif
#include "../control/magma_threadsetting.h"  // internal header

#define REAL

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing ssyr2k_batched
*/
int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    real_Double_t   gflops, magma_perf, magma_time, cpu_perf, cpu_time;
    float          error, magma_error, normalize, work[1];
    magma_int_t N, K;
    magma_int_t An, Ak, Bn, Bk;
    magma_int_t sizeA, sizeB, sizeC;
    magma_int_t lda, ldb, ldc, ldda, lddb, lddc;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    int status = 0;
    magma_int_t batchCount;

    float *h_A, *h_B, *h_C, *h_Cmagma;
    float *d_A, *d_B, *d_C;
    float **d_A_array = NULL;
    float **d_B_array = NULL;
    float **d_C_array = NULL;
    float **h_A_array = NULL, **h_B_array = NULL, **h_C_array = NULL;

    float c_neg_one = MAGMA_S_NEG_ONE;
    float alpha = MAGMA_S_MAKE(  0.29, -0.86 );
    float beta  = MAGMA_D_MAKE( -0.48,  0.38 );
    
    magma_opts opts( MagmaOptsBatched );
    opts.parse_opts( argc, argv );
    opts.lapack |= opts.check;
    batchCount = opts.batchcount;
    
    float *Anorm, *Bnorm, *Cnorm;
    TESTING_CHECK( magma_smalloc_cpu( &Anorm, batchCount ));
    TESTING_CHECK( magma_smalloc_cpu( &Bnorm, batchCount ));
    TESTING_CHECK( magma_smalloc_cpu( &Cnorm, batchCount ));
    
    TESTING_CHECK( magma_malloc_cpu((void**)&h_A_array, batchCount*sizeof(float*)) );
    TESTING_CHECK( magma_malloc_cpu((void**)&h_B_array, batchCount*sizeof(float*)) );
    TESTING_CHECK( magma_malloc_cpu((void**)&h_C_array, batchCount*sizeof(float*)) );

    TESTING_CHECK( magma_malloc((void**)&d_A_array, batchCount*sizeof(float*)) );
    TESTING_CHECK( magma_malloc((void**)&d_B_array, batchCount*sizeof(float*)) );
    TESTING_CHECK( magma_malloc((void**)&d_C_array, batchCount*sizeof(float*)) );
    
    // See testing_sgemm about tolerance.
    float eps = lapackf77_slamch("E");
    float tol = 3*eps;
    
    #ifdef COMPLEX
    if (opts.transA == MagmaTrans) {
        opts.transA = MagmaConjTrans;
        printf("%% WARNING: transA = MagmaTrans changed to MagmaConjTrans\n");
    }
    #endif
    
    printf("%% If running lapack (option --lapack), MAGMA error is computed\n"
           "%% relative to CPU BLAS result.\n\n"
           "%% uplo = %s, trans = %s\n",
           lapack_uplo_const(opts.uplo),
           lapack_trans_const(opts.transA));
    
    printf("%% BatchCount     N     K   MAGMA Gflop/s (ms)   CPU Gflop/s (ms)   MAGMA error\n");
    printf("%%=============================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N = opts.nsize[itest];
            K = opts.ksize[itest];
            gflops = (batchCount * FLOPS_SSYR2K( K, N )) / 1e9;

            if ( opts.transA == MagmaNoTrans ) {
                lda = An = N;
                ldb = Bn = N;
                Ak = K;
                Bk = K;
            }
            else {
                lda = An = K;
                ldb = Bn = K;
                Ak = N;
                Bk = N;
            }
            
            ldc = N;
            
            ldda = magma_roundup( lda, opts.align );  // multiple of 32 by default
            lddb = magma_roundup( ldb, opts.align );  // multiple of 32 by default
            lddc = magma_roundup( ldc, opts.align );  // multiple of 32 by default

            sizeA = lda*Ak*batchCount;
            sizeB = ldb*Bk*batchCount;
            sizeC = ldc*N*batchCount;
            
            TESTING_CHECK( magma_smalloc_cpu(&h_A, sizeA) );
            TESTING_CHECK( magma_smalloc_cpu(&h_B, sizeB) );
            TESTING_CHECK( magma_smalloc_cpu(&h_C, sizeC) );
            TESTING_CHECK( magma_smalloc_cpu(&h_Cmagma, sizeC) );

            TESTING_CHECK( magma_smalloc(&d_A, ldda*Ak*batchCount) );
            TESTING_CHECK( magma_smalloc(&d_B, lddb*Bk*batchCount) );
            TESTING_CHECK( magma_smalloc(&d_C, lddc*N*batchCount)  );

            /* Initialize the matrices */
            lapackf77_slarnv( &ione, ISEED, &sizeA, h_A );
            lapackf77_slarnv( &ione, ISEED, &sizeB, h_B );
            lapackf77_slarnv( &ione, ISEED, &sizeC, h_C );
            
            // Compute norms for error
            for (int s = 0; s < batchCount; ++s) {
                Anorm[s] = lapackf77_slange( "F", &An, &Ak, &h_A[s*lda*Ak], &lda, work );
                Bnorm[s] = lapackf77_slange( "F", &Bn, &Bk, &h_B[s*ldb*Bk], &ldb, work );
                Cnorm[s] = safe_lapackf77_slansy( "F", lapack_uplo_const(opts.uplo), &N, &h_C[s*ldc*N], &ldc, work );
            }

            /* =====================================================================
               Performs operation using MAGMABLAS
               =================================================================== */
            magma_ssetmatrix( An, Ak*batchCount, h_A, lda, d_A, ldda, opts.queue );
            magma_ssetmatrix( Bn, Bk*batchCount, h_B, ldb, d_B, lddb, opts.queue );
            magma_ssetmatrix( N, N*batchCount, h_C, ldc, d_C, lddc, opts.queue );
            
            magma_sset_pointer( d_A_array, d_A, ldda, 0, 0, ldda*Ak, batchCount, opts.queue );
            magma_sset_pointer( d_B_array, d_B, lddb, 0, 0, lddb*Bk, batchCount, opts.queue );
            magma_sset_pointer( d_C_array, d_C, lddc, 0, 0, lddc*N,  batchCount, opts.queue );

            magma_time = magma_sync_wtime( opts.queue );
            magmablas_ssyr2k_batched(opts.uplo, opts.transA, N, K, 
                             alpha, d_A_array, ldda, 
                                    d_B_array, lddb, 
                             beta,  d_C_array, lddc, batchCount, opts.queue );
            magma_time = magma_sync_wtime( opts.queue ) - magma_time;
            magma_perf = gflops / magma_time;
            magma_sgetmatrix( N, N*batchCount, d_C, lddc, h_Cmagma, ldc, opts.queue );
            
            /* =====================================================================
               Performs operation using CPU BLAS
               =================================================================== */
            if ( opts.lapack ) {
                // populate pointer arrays on the host
                for(int i = 0; i < batchCount; i++){
                    h_A_array[i] = h_A + i*lda*Ak;
                    h_B_array[i] = h_B + i*ldb*Bk;
                    h_C_array[i] =  h_C + i*ldc*N;
                }
                cpu_time = magma_wtime();
                blas_ssyr2k_batched(
                    opts.uplo, opts.transA, 
                    N, K, 
                    alpha, h_A_array, lda, 
                           h_B_array, ldb, 
                    beta,  h_C_array, ldc, batchCount );
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
            }
            
            /* =====================================================================
               Check the result
               =================================================================== */
            if ( opts.lapack ) {
                // compute error compared lapack
                // error = |dC - C| / (2*gamma_{k+2}|A||B| + gamma_2|Cin|)
                magma_error = 0;

                for (int s=0; s < batchCount; s++) {
                    normalize = 2*sqrt(float(K+2))*Anorm[s]*Bnorm[s] + 2*Cnorm[s];
                    if (normalize == 0)
                        normalize = 1;
                    magma_int_t Csize = ldc * N;
                    blasf77_saxpy( &Csize, &c_neg_one, &h_C[s*ldc*N], &ione, &h_Cmagma[s*ldc*N], &ione );
                    error = safe_lapackf77_slansy( "F", lapack_uplo_const(opts.uplo), &N, &h_Cmagma[s*ldc*N], &ldc, work )
                          / normalize;
                    magma_error = magma_max_nan( error, magma_error );
                }
                
                bool okay = (magma_error < tol);
                status += ! okay;
                printf("  %10lld %5lld %5lld   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e  %s\n",
                       (long long) batchCount, (long long) N, (long long) K,
                       magma_perf,  1000.*magma_time,
                       cpu_perf,    1000.*cpu_time,
                       magma_error, (okay ? "ok" : "failed"));
            }
            else {
                printf("  %10lld %5lld %5lld   %7.2f (%7.2f)     ---   (  ---  )     ---\n",
                       (long long) batchCount, (long long) N, (long long) K,
                       magma_perf,  1000.*magma_time);
            }
            
            magma_free_cpu( h_A  );
            magma_free_cpu( h_B  );
            magma_free_cpu( h_C  );
            magma_free_cpu( h_Cmagma  );

            magma_free( d_A );
            magma_free( d_B );
            magma_free( d_C );

            fflush( stdout);
        }
        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }
    
    magma_free_cpu( Anorm );
    magma_free_cpu( Bnorm );
    magma_free_cpu( Cnorm );
    magma_free_cpu( h_A_array );
    magma_free_cpu( h_B_array );
    magma_free_cpu( h_C_array );

    magma_free( d_A_array );
    magma_free( d_B_array );
    magma_free( d_C_array );
    
    opts.cleanup();
    TESTING_CHECK( magma_finalize() );
    return status;
}
