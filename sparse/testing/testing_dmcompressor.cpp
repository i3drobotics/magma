/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/testing/testing_zmcompressor.cpp, normal z -> d, Sat Mar 27 20:33:09 2021
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
#include "testings.h"


/* ////////////////////////////////////////////////////////////////////////////
   -- testing any solver
*/
int main(  int argc, char** argv )
{
    magma_int_t info = 0;
    TESTING_CHECK( magma_init() );
    magma_print_environment();

    magma_dopts zopts;
    magma_queue_t queue=NULL;
    magma_queue_create( 0, &queue );

    real_Double_t res;
    magma_d_matrix A={Magma_CSR}, AT={Magma_CSR}, A2={Magma_CSR}, 
    B={Magma_CSR}, dB={Magma_CSR};
    
    int i=1;
    real_Double_t start, end;
    TESTING_CHECK( magma_dparse_opts( argc, argv, &zopts, &i, queue ));

    B.blocksize = zopts.blocksize;
    B.alignment = zopts.alignment;

    while( i < argc ) {
        if ( strcmp("LAPLACE2D", argv[i]) == 0 && i+1 < argc ) {   // Laplace test
            i++;
            magma_int_t laplace_size = atoi( argv[i] );
            TESTING_CHECK( magma_dm_5stencil(  laplace_size, &A, queue ));
        } else {                        // file-matrix test
            TESTING_CHECK( magma_d_csr_mtx( &A,  argv[i], queue ));
        }

        printf( "\n# matrix info: %lld-by-%lld with %lld nonzeros\n\n",
                (long long) A.num_rows, (long long) A.num_cols, (long long) A.nnz );

        // scale matrix
        TESTING_CHECK( magma_dmscale( &A, zopts.scaling, queue ));

        // remove nonzeros in matrix
        start = magma_sync_wtime( queue );
        for (int j=0; j < 10; j++) {
            TESTING_CHECK( magma_dmcsrcompressor( &A, queue ));
        }
        end = magma_sync_wtime( queue );
        printf( " > MAGMA CPU: %.2e seconds.\n", (end-start)/10 );
        // transpose
        TESTING_CHECK( magma_dmtranspose( A, &AT, queue ));

        // convert, copy back and forth to check everything works
        TESTING_CHECK( magma_dmconvert( AT, &B, Magma_CSR, Magma_CSR, queue ));
        magma_dmfree(&AT, queue );
        TESTING_CHECK( magma_dmtransfer( B, &dB, Magma_CPU, Magma_DEV, queue ));
        magma_dmfree(&B, queue );

        start = magma_sync_wtime( queue );
        for (int j=0; j < 10; j++) {
            TESTING_CHECK( magma_dmcsrcompressor_gpu( &dB, queue ));
        }
        end = magma_sync_wtime( queue );
        printf( " > MAGMA GPU: %.2e seconds.\n", (end-start)/10 );


        TESTING_CHECK( magma_dmtransfer( dB, &B, Magma_DEV, Magma_CPU, queue ));
        magma_dmfree(&dB, queue );
        TESTING_CHECK( magma_dmconvert( B, &AT, Magma_CSR, Magma_CSR, queue ));
        magma_dmfree(&B, queue );

        // transpose back
        TESTING_CHECK( magma_dmtranspose( AT, &A2, queue ));
        magma_dmfree(&AT, queue );
        TESTING_CHECK( magma_dmdiff( A, A2, &res, queue ));
        printf("%% ||A-B||_F = %8.2e\n", res);
        if ( res < .000001 )
            printf("%% tester matrix compressor:  ok\n");
        else
            printf("%% tester matrix compressor:  failed\n");

        magma_dmfree(&A, queue );
        magma_dmfree(&A2, queue );

        i++;
    }
    
    magma_queue_destroy( queue );
    TESTING_CHECK( magma_finalize() );
    return info;
}
