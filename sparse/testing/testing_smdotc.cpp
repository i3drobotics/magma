/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/testing/testing_zmdotc.cpp, normal z -> s, Sat Mar 27 20:33:12 2021
       @author Hartwig Anzt
*/

// includes, system
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

// includes, project
#include "magma_v2.h"
#include "magmasparse.h"
#include "magma_operators.h"
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- testing zdot
*/
int main(  int argc, char** argv )
{
    magma_int_t info = 0;
    magma_queue_t queue=NULL;
    magma_queue_create( 0, &queue );

    const float one  = MAGMA_S_MAKE(1.0, 0.0);
    const float zero = MAGMA_S_MAKE(0.0, 0.0);
    float alpha;

    TESTING_CHECK( magma_init() );
    magma_print_environment();

    magma_s_matrix a={Magma_CSR}, b={Magma_CSR}, x={Magma_CSR}, y={Magma_CSR}, skp={Magma_CSR};

    printf("%%================================================================================================================================================\n");
    printf("\n");
    printf("%%            |     runtime            |       GFLOPS\n");
    printf("%% n num_vecs |  CUDOT    MAGMA MDOTC  |  CUDOT    MAGMA MDOTC\n");
    printf("%%------------------------------------------------------------------------------------------------------------------------------------------------\n");
    printf("\n");

    for( magma_int_t num_vecs=2; num_vecs < 9; num_vecs += 2 ) {
        for( magma_int_t n=1000000; n < 5000001; n += 1000000 ) {
            int iters = 10;
            float computations = ( n * iters * num_vecs);

            #ifndef ENABLE_TIMER
            #define ENABLE_TIMER
            #endif
            
            #ifdef ENABLE_TIMER
            real_Double_t mdot1, mdot2, cudot1, cudot2;
            real_Double_t mdot_time, cudot_time;
            #endif

            TESTING_CHECK( magma_svinit( &a, Magma_DEV, n, num_vecs, one, queue ));
            TESTING_CHECK( magma_svinit( &b, Magma_DEV, num_vecs, 1, one, queue ));
            int min_ten = min(num_vecs, 15);
            TESTING_CHECK( magma_svinit( &x, Magma_DEV, min_ten, n, one, queue ));
            TESTING_CHECK( magma_svinit( &y, Magma_DEV, min_ten, n, one, queue ));
            TESTING_CHECK( magma_svinit( &skp, Magma_DEV, num_vecs, 1, zero, queue ));

            // warm up
            TESTING_CHECK( magma_sgemvmdot( n, num_vecs, a.dval, b.dval, x.dval, y.dval, skp.dval, queue ));

            // CUDOT
            #ifdef ENABLE_TIMER
            cudot1 = magma_sync_wtime( queue );
            #endif
            for( int h=0; h < iters; h++) {
                for( int l=0; l < num_vecs/2; l++) {
                    alpha = magma_sdot( n, a.dval, 1, b.dval, 1, queue );
                }
            }
            #ifdef ENABLE_TIMER
            cudot2 = magma_sync_wtime( queue );
            cudot_time=cudot2-cudot1;
            #endif
          
            // MAGMA MDOTC
            #ifdef ENABLE_TIMER
            mdot1 = magma_sync_wtime( queue );
            #endif
            for( int h=0; h < iters; h++) {
                if( num_vecs == 2 ){
                    magma_smdotc1( n, a.dval, b.dval, x.dval, y.dval, skp.dval, queue );
                }
                else if( num_vecs == 4 ){
                    magma_smdotc1( n, a.dval, b.dval, x.dval, y.dval, skp.dval, queue );
                    magma_smdotc1( n, a.dval, b.dval, x.dval, y.dval, skp.dval, queue );
                }
                else if( num_vecs == 6 ){
                    magma_smdotc3( n, a.dval, b.dval, a.dval+n, b.dval+n, a.dval+2*n, b.dval+2*n, x.dval, y.dval, skp.dval, queue );
                }
                else if( num_vecs == 8 ){
                    magma_smdotc3( n, a.dval, b.dval, a.dval+n, b.dval+n, a.dval+2*n, b.dval+2*n, x.dval, y.dval, skp.dval, queue );
                }
                else{
                    printf("error: not supported.\n");
                }
            }
            #ifdef ENABLE_TIMER
            mdot2 = magma_sync_wtime( queue );
            mdot_time=mdot2-mdot1;
            #endif
           
            //Chronometry
            #ifdef ENABLE_TIMER
            printf("%lld  %lld  %e  %e  %e  %e\n",
                    (long long) n, (long long) num_vecs,
                    cudot_time/iters,
                    (mdot_time)/iters,
                    computations/(cudot_time*1e9),
                    computations/(mdot_time*1e9));
            #endif

            magma_smfree(&a, queue );
            magma_smfree(&b, queue );
            magma_smfree(&x, queue );
            magma_smfree(&y, queue );
            magma_smfree(&skp, queue );
        }

        printf("%%================================================================================================================================================\n");
        printf("\n");
        printf("\n");
    }
    
    // use alpha to silence compiler warnings
    if ( magma_s_isnan( (float) real( alpha ))) {
        info = -1;
    }

    magma_queue_destroy( queue );
    TESTING_CHECK( magma_finalize() );
    return info;
}
