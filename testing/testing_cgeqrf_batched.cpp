/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Tingxing Dong
       @author Azzam Haidar

       @generated from testing/testing_zgeqrf_batched.cpp, normal z -> c, Sat Mar 27 20:32:18 2021

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
#include "../control/magma_threadsetting.h"  // internal header
#endif

void get_QR_error(magma_int_t M, magma_int_t N, magma_int_t min_mn,
                    magmaFloatComplex *h_R,  magmaFloatComplex *h_A, magma_int_t lda,
                    magmaFloatComplex *tau,
                    magmaFloatComplex *Q,  magma_int_t ldq,
                    magmaFloatComplex *R,  magma_int_t ldr,
                    magmaFloatComplex *h_work,  magma_int_t lwork,
                    float *work, float *error, float *error2)
{
    /* h_R:input the factorized matrix by lapack QR,
       h_A:input the original matrix copy
       tau: input
    */
    
    const float             d_neg_one = MAGMA_D_NEG_ONE;
    const float             d_one     = MAGMA_D_ONE;
    const magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    const magmaFloatComplex c_one     = MAGMA_C_ONE;
    const magmaFloatComplex c_zero    = MAGMA_C_ZERO;
    float           Anorm;
    
    magma_int_t info;
    
    // generate M by K matrix Q, where K = min(M,N)
    lapackf77_clacpy( "Lower", &M, &min_mn, h_R, &lda, Q, &ldq );
    lapackf77_cungqr( &M, &min_mn, &min_mn, Q, &ldq, tau, h_work, &lwork, &info );
    assert( info == 0 );
    
    // copy K by N matrix R
    lapackf77_claset( "Lower", &min_mn, &N, &c_zero, &c_zero, R, &ldr );
    lapackf77_clacpy( "Upper", &min_mn, &N, h_R, &lda,        R, &ldr );
    
    // error = || R - Q^H*A || / (N * ||A||)
    blasf77_cgemm( "Conj", "NoTrans", &min_mn, &N, &M,
    &c_neg_one, Q, &ldq, h_A, &lda, &c_one, R, &ldr );
    
    Anorm = lapackf77_clange( "1", &M,      &N, h_A, &lda, work );
    *error = lapackf77_clange( "1", &min_mn, &N, R,   &ldr, work );
    
    if ( N > 0 && Anorm > 0 )
        *error /= (N*Anorm);
    
    // set R = I (K by K identity), then R = I - Q^H*Q
    // error = || I - Q^H*Q || / N
    lapackf77_claset( "Upper", &min_mn, &min_mn, &c_zero, &c_one, R, &ldr );
    blasf77_cherk( "Upper", "Conj", &min_mn, &M, &d_neg_one, Q, &ldq, &d_one, R, &ldr );
    *error2 = safe_lapackf77_clanhe( "1", "Upper", &min_mn, R, &ldr, work );
    if ( N > 0 )
        *error2 /= N;
}


/* ////////////////////////////////////////////////////////////////////////////
   -- Testing cgeqrf_batched
*/
int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    real_Double_t    gflops, magma_perf, magma_time, cublas_perf=0, cublas_time=0, cpu_perf, cpu_time;
    float           magma_error, cublas_error, magma_error2, cublas_error2;

    magmaFloatComplex *h_A, *h_R, *h_Amagma, *tau, *h_work, tmp[1], unused[1];
    magmaFloatComplex *d_A, *dtau_magma, *dtau_cublas;

    magmaFloatComplex **dA_array = NULL;
    magmaFloatComplex **dtau_array = NULL;

    magma_int_t   *dinfo_magma, *dinfo_cublas;

    magma_int_t M, N, lda, ldda, lwork, n2, info, min_mn;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    int status = 0;

    magma_int_t batchCount;
    magma_int_t column;

    magma_opts opts( MagmaOptsBatched );
    opts.parse_opts( argc, argv );
    batchCount = opts.batchcount;

    float tol = opts.tolerance * lapackf77_slamch("E");
    
    printf("%% BatchCount   M     N   MAGMA Gflop/s (ms)   CUBLAS Gflop/s (ms)    CPU Gflop/s (ms)   |R - Q^H*A|_mag   |I - Q^H*Q|_mag   |R - Q^H*A|_cub   |I - Q^H*Q|_cub\n");
    printf("%%============================================================================================================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            M     = opts.msize[itest];
            N     = opts.nsize[itest];
            min_mn = min(M, N);
            lda    = M;
            n2     = lda*N * batchCount;
            ldda = M;
            ldda   = magma_roundup( M, opts.align );  // multiple of 32 by default

            gflops = (FLOPS_CGEQRF( M, N ) + FLOPS_CGEQRT( M, N )) / 1e9 * batchCount;

            /* Allocate memory for the matrix */
            TESTING_CHECK( magma_cmalloc_cpu( &tau,   min_mn * batchCount ));
            TESTING_CHECK( magma_cmalloc_cpu( &h_A,   n2     ));
            TESTING_CHECK( magma_cmalloc_cpu( &h_Amagma,   n2     ));
            TESTING_CHECK( magma_cmalloc_pinned( &h_R,   n2     ));
        
            TESTING_CHECK( magma_cmalloc( &d_A,   ldda*N * batchCount ));

            TESTING_CHECK( magma_cmalloc( &dtau_magma,  min_mn * batchCount ));
            TESTING_CHECK( magma_cmalloc( &dtau_cublas, min_mn * batchCount ));

            TESTING_CHECK( magma_imalloc( &dinfo_magma,  batchCount ));
            TESTING_CHECK( magma_imalloc( &dinfo_cublas, batchCount ));

            TESTING_CHECK( magma_malloc( (void**) &dA_array,   batchCount * sizeof(magmaFloatComplex*) ));
            TESTING_CHECK( magma_malloc( (void**) &dtau_array, batchCount * sizeof(magmaFloatComplex*) ));
        
            // to determine the size of lwork
            lwork = -1;
            lapackf77_cgeqrf( &M, &N, unused, &M, unused, tmp, &lwork, &info );
            lwork = (magma_int_t)MAGMA_C_REAL( tmp[0] );
            lwork = max(lwork, N*N);
           
            TESTING_CHECK( magma_cmalloc_cpu( &h_work, lwork * batchCount ));

            column = N * batchCount;
            /* Initialize the matrix */
            lapackf77_clarnv( &ione, ISEED, &n2, h_A );
            lapackf77_clacpy( MagmaFullStr, &M, &column, h_A, &lda, h_R, &lda );
       
            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            magma_csetmatrix( M, column, h_R, lda,  d_A, ldda, opts.queue );
            magma_cset_pointer( dA_array, d_A, 1, 0, 0, ldda*N, batchCount, opts.queue );
            magma_cset_pointer( dtau_array, dtau_magma, 1, 0, 0, min_mn, batchCount, opts.queue );
    
            magma_time = magma_sync_wtime( opts.queue );
    
            info = magma_cgeqrf_batched(M, N, dA_array, ldda, dtau_array, dinfo_magma, batchCount, opts.queue);

            magma_time = magma_sync_wtime( opts.queue ) - magma_time;
            magma_perf = gflops / magma_time;

            magma_cgetmatrix( M, column, d_A, ldda, h_Amagma, lda, opts.queue );

            if (info != 0) {
                printf("magma_cgeqrf_batched returned error %lld: %s.\n",
                       (long long) info, magma_strerror( info ));
            }
            
            /* ====================================================================
               Performs operation using CUBLAS
               =================================================================== */

            /* cublasCgeqrfBatched is only available from CUBLAS v6.5 */
            #if CUDA_VERSION >= 6050
            magma_csetmatrix( M, column, h_R, lda,  d_A, ldda, opts.queue );
            magma_cset_pointer( dA_array, d_A, 1, 0, 0, ldda*N, batchCount, opts.queue );
            magma_cset_pointer( dtau_array, dtau_cublas, 1, 0, 0, min_mn, batchCount, opts.queue );

            cublas_time = magma_sync_wtime( opts.queue );
    
            int cublas_info;  // not magma_int_t
            cublasCgeqrfBatched( opts.handle, int(M), int(N),
                                 dA_array, int(ldda), dtau_array,
                                 &cublas_info, int(batchCount) );

            cublas_time = magma_sync_wtime( opts.queue ) - cublas_time;
            cublas_perf = gflops / cublas_time;

            if (cublas_info != 0) {
                printf("cublasCgeqrfBatched returned error %lld: %s.\n",
                       (long long) cublas_info, magma_strerror( cublas_info ));
            }
            #endif

            /* =====================================================================
               Performs operation using LAPACK
               =================================================================== */
            if ( opts.lapack ) {
                cpu_time = magma_wtime();
                // #define BATCHED_DISABLE_PARCPU
                #if !defined (BATCHED_DISABLE_PARCPU) && defined(_OPENMP)
                magma_int_t nthreads = magma_get_lapack_numthreads();
                magma_set_lapack_numthreads(1);
                magma_set_omp_numthreads(nthreads);
                #pragma omp parallel for schedule(dynamic)
                #endif
                for (magma_int_t s=0; s < batchCount; s++)
                {
                    magma_int_t locinfo;
                    lapackf77_cgeqrf(&M, &N, h_A + s * lda * N, &lda, tau + s * min_mn, h_work + s * lwork, &lwork, &locinfo);
                    if (locinfo != 0) {
                        printf("lapackf77_cgeqrf matrix %lld returned error %lld: %s.\n",
                               (long long) s, (long long) locinfo, magma_strerror( locinfo ));
                    }
                }

                #if !defined (BATCHED_DISABLE_PARCPU) && defined(_OPENMP)
                    magma_set_lapack_numthreads(nthreads);
                #endif
                
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                if (info != 0) {
                    printf("lapackf77_cgeqrf returned error %lld: %s.\n",
                           (long long) info, magma_strerror( info ));
                }
                printf("%10lld %5lld %5lld    %7.2f (%7.2f)     %7.2f (%7.2f)   %7.2f (%7.2f)",
                       (long long) batchCount, (long long) M, (long long) N,
                       magma_perf,  1000.*magma_time,
                       cublas_perf, 1000.*cublas_time,
                       cpu_perf,    1000.*cpu_time );
            }
            else {
                printf("%10lld %5lld %5lld    %7.2f (%7.2f)     %7.2f (%7.2f)     ---  (  ---  )",
                       (long long) batchCount, (long long) M, (long long) N,
                       magma_perf,  1000.*magma_time,
                       cublas_perf, 1000.*cublas_time );
            }
            
            if (opts.check) {
                /* =====================================================================
                   Check the MAGMA and CUBLAS results by computing residuals 
                   =================================================================== */
                magma_int_t ldq = M;
                magma_int_t ldr = min_mn;
                magmaFloatComplex *Q, *R;
                float *work;

                TESTING_CHECK( magma_cmalloc_cpu( &Q,    ldq*min_mn ));  // M by K
                TESTING_CHECK( magma_cmalloc_cpu( &R,    ldr*N ));       // K by N
                TESTING_CHECK( magma_smalloc_cpu( &work, min_mn ));

                /* check magma result */
                magma_error  = 0;
                magma_error2 = 0;
                magma_cgetvector(min_mn*batchCount, dtau_magma, 1, tau, 1, opts.queue );
                for (int i=0; i < batchCount; i++)
                {
                    float err, err2;
                    get_QR_error(M, N, min_mn,
                             h_Amagma + i*lda*N, h_R + i*lda*N, lda, tau + i*min_mn,
                             Q, ldq, R, ldr, h_work, lwork,
                             work, &err, &err2);
                    magma_error  = magma_max_nan( err,  magma_error  );
                    magma_error2 = magma_max_nan( err2, magma_error2 );
                }

                /* check cublas result */
                cublas_error  = 0;
                cublas_error2 = 0;
                #if CUDA_VERSION >= 6050
                magma_cgetvector(min_mn*batchCount, dtau_cublas, 1, tau, 1, opts.queue );
                magma_cgetmatrix( M, column, d_A, ldda, h_A, lda, opts.queue );
                for (int i=0; i < batchCount; i++)
                {
                    float err, err2;
                    get_QR_error(M, N, min_mn,
                             h_A + i*lda*N, h_R + i*lda*N, lda, tau + i*min_mn,
                             Q, ldq, R, ldr, h_work, lwork,
                             work, &err, &err2);
                    cublas_error  = magma_max_nan( err,  cublas_error  );
                    cublas_error2 = magma_max_nan( err2, cublas_error2 );
                }
                #endif

                magma_free_cpu( Q    );  Q    = NULL;
                magma_free_cpu( R    );  R    = NULL;
                magma_free_cpu( work );  work = NULL;

                bool okay = (magma_error < tol && magma_error2 < tol);
                status += ! okay;

                printf("   %15.2e   %15.2e   %15.2e   %15.2e   %s\n",
                       magma_error, magma_error2,
                       cublas_error, cublas_error2,
                       (okay ? "ok" : "failed") );
            }
            else {
                printf("\n");
            }
            
            magma_free_cpu( tau    );
            magma_free_cpu( h_A    );
            magma_free_cpu( h_Amagma );
            magma_free_cpu( h_work );
            magma_free_pinned( h_R    );
        
            magma_free( d_A   );
            magma_free( dtau_magma  );
            magma_free( dtau_cublas );

            magma_free( dinfo_magma );
            magma_free( dinfo_cublas );

            magma_free( dA_array   );
            magma_free( dtau_array  );

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
