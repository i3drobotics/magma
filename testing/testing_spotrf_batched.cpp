/*
   -- MAGMA (version 2.0) --
   Univ. of Tennessee, Knoxville
   Univ. of California, Berkeley
   Univ. of Colorado, Denver
   @date

   @author Azzam Haidar
   @author Tingxing Dong

   @generated from testing/testing_zpotrf_batched.cpp, normal z -> s, Sat Mar 27 20:32:19 2021
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

/* ////////////////////////////////////////////////////////////////////////////
   -- Testing spotrf_batched
*/

int main( int argc, char** argv)
{
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    real_Double_t   gflops, gpu_perf, gpu_time, cpu_perf, cpu_time;
    float *h_A, *h_R;
    float *d_A;
    magma_int_t N, n2, lda, ldda, info;
    float c_neg_one = MAGMA_S_NEG_ONE;
    magma_int_t ione     = 1;
    magma_int_t ISEED[4] = {0,0,0,1};
    float      work[1], error;
    int status = 0;
    float **d_A_array = NULL;
    magma_int_t *dinfo_magma;
    magma_int_t *hinfo_magma;

    magma_int_t batchCount;

    magma_opts opts( MagmaOptsBatched );
    opts.parse_opts( argc, argv );
    opts.lapack |= opts.check;  // check (-c) implies lapack (-l)
    batchCount = opts.batchcount;
    float tol = opts.tolerance * lapackf77_slamch("E");

    magma_queue_t queue = opts.queue;

    printf("%% BatchCount   N    CPU Gflop/s (ms)    GPU Gflop/s (ms)   ||R_magma - R_lapack||_F / ||R_lapack||_F\n");
    printf("%%===================================================================================================\n");
    for( int itest = 0; itest < opts.ntest; ++itest ) {
        for( int iter = 0; iter < opts.niter; ++iter ) {
            N   = opts.nsize[itest];
            lda = N;
            ldda = magma_roundup( N, opts.align );  // multiple of 32 by default
            n2  = lda* N  * batchCount;

            gflops = batchCount * FLOPS_SPOTRF( N ) / 1e9;

            TESTING_CHECK( magma_imalloc_cpu( &hinfo_magma, batchCount ));
            TESTING_CHECK( magma_smalloc_cpu( &h_A, n2 ));
            TESTING_CHECK( magma_smalloc_pinned( &h_R, n2 ));
            TESTING_CHECK( magma_smalloc( &d_A, ldda * N * batchCount ));
            TESTING_CHECK( magma_imalloc( &dinfo_magma,  batchCount ));
            
            TESTING_CHECK( magma_malloc( (void**) &d_A_array, batchCount * sizeof(float*) ));

            /* Initialize the matrix */
            lapackf77_slarnv( &ione, ISEED, &n2, h_A );
            for (int i=0; i < batchCount; i++)
            {
                magma_smake_hpd( N, h_A + i * lda * N, lda ); // need modification
            }
            
            magma_int_t columns = N * batchCount;
            lapackf77_slacpy( MagmaFullStr, &N, &(columns), h_A, &lda, h_R, &lda );

            magma_ssetmatrix( N, columns, h_A, lda, d_A, ldda, opts.queue );

            /* ====================================================================
               Performs operation using MAGMA
               =================================================================== */
            magma_memset( dinfo_magma, 0, batchCount * sizeof(magma_int_t) );

            magma_sset_pointer( d_A_array, d_A, ldda, 0, 0, ldda * N, batchCount, queue );
            gpu_time = magma_sync_wtime( opts.queue );
            info = magma_spotrf_batched( opts.uplo, N, d_A_array, ldda, dinfo_magma, batchCount, queue);
            gpu_time = magma_sync_wtime( opts.queue ) - gpu_time;
            gpu_perf = gflops / gpu_time;
            magma_getvector( batchCount, sizeof(magma_int_t), dinfo_magma, 1, hinfo_magma, 1, opts.queue );
            for (int i=0; i < batchCount; i++)
            {
                if (hinfo_magma[i] != 0 ) {
                    printf("magma_spotrf_batched matrix %lld returned diag error %lld\n",
                            (long long) i, (long long) hinfo_magma[i] );
                    status = -1;
                }
            }
            if (info != 0) {
                //printf("magma_spotrf_batched returned argument error %lld: %s.\n", (long long) info, magma_strerror( info ));
                status = -1;
            }                
            if (status == -1)
                goto cleanup;


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
                    lapackf77_spotrf( lapack_uplo_const(opts.uplo), &N, h_A + s * lda * N, &lda, &locinfo );
                    if (locinfo != 0) {
                        printf("lapackf77_spotrf matrix %lld returned error %lld: %s.\n",
                               (long long) s, (long long) locinfo, magma_strerror( locinfo ));
                    }
                }

                #if !defined (BATCHED_DISABLE_PARCPU) && defined(_OPENMP)
                    magma_set_lapack_numthreads(nthreads);
                #endif
            
                cpu_time = magma_wtime() - cpu_time;
                cpu_perf = gflops / cpu_time;
                
                /* =====================================================================
                   Check the result compared to LAPACK
                   =================================================================== */
                magma_sgetmatrix( N, columns, d_A, ldda, h_R, lda, opts.queue );
                magma_int_t NN = lda*N;
                const char* uplo = lapack_uplo_const(opts.uplo);
                error = 0;
                for (int i=0; i < batchCount; i++)
                {
                    float Anorm, err;
                    blasf77_saxpy(&NN, &c_neg_one, h_A + i * lda*N, &ione, h_R + i * lda*N, &ione);
                    Anorm = safe_lapackf77_slansy("f", uplo, &N, h_A + i * lda*N, &lda, work);
                    err   = safe_lapackf77_slansy("f", uplo, &N, h_R + i * lda*N, &lda, work)
                          / Anorm;
                    if (std::isnan(err) || std::isinf(err)) {
                        error = err;
                        break;
                    }
                    error = max( err, error );
                }
                bool okay = (error < tol);
                status += ! okay;
                
                printf("%10lld %5lld   %7.2f (%7.2f)   %7.2f (%7.2f)   %8.2e   %s\n",
                       (long long) batchCount, (long long) N, cpu_perf, cpu_time*1000., gpu_perf, gpu_time*1000.,
                       error, (okay ? "ok" : "failed"));
            }
            else {
                printf("%10lld %5lld     ---   (  ---  )   %7.2f (%7.2f)     ---\n",
                       (long long) batchCount, (long long) N, gpu_perf, gpu_time*1000. );
            }
cleanup:
            magma_free_cpu( hinfo_magma );
            magma_free_cpu( h_A );
            magma_free_pinned( h_R );
            magma_free( d_A );
            magma_free( d_A_array );
            magma_free( dinfo_magma );
            if (status == -1)
                break;
            fflush( stdout );
        }
        if (status == -1)
            break;

        if ( opts.niter > 1 ) {
            printf( "\n" );
        }
    }

    opts.cleanup();
    TESTING_CHECK( magma_finalize() );
    return status;
}
