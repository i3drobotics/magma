/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/testing/testing_zmadd.cpp, normal z -> d, Sat Mar 27 20:33:13 2021
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
   -- testing csr matrix add
*/
int main(  int argc, char** argv )
{
    magma_int_t info = 0;
    TESTING_CHECK( magma_init() );
    magma_print_environment();
    
    magma_queue_t queue=NULL;
    magma_queue_create( 0, &queue );

    real_Double_t res;
    magma_d_matrix A={Magma_CSR}, B={Magma_CSR}, B2={Magma_CSR}, 
    dA={Magma_CSR}, dB={Magma_CSR}, dC={Magma_CSR};

    double one = MAGMA_D_MAKE(1.0, 0.0);
    double mone = MAGMA_D_MAKE(-1.0, 0.0);

    magma_int_t i=1;

    if ( strcmp("LAPLACE2D", argv[i]) == 0 && i+1 < argc ) {   // Laplace test
        i++;
        magma_int_t laplace_size = atoi( argv[i] );
        TESTING_CHECK( magma_dm_5stencil(  laplace_size, &A, queue ));
    } else {                        // file-matrix test
        TESTING_CHECK( magma_d_csr_mtx( &A,  argv[i], queue ));
    }
    printf("%% matrix info: %lld-by-%lld with %lld nonzeros\n",
            (long long) A.num_rows, (long long) A.num_cols, (long long) A.nnz );
    i++;

    if ( strcmp("LAPLACE2D", argv[i]) == 0 && i+1 < argc ) {   // Laplace test
        i++;
        magma_int_t laplace_size = atoi( argv[i] );
        TESTING_CHECK( magma_dm_5stencil(  laplace_size, &B, queue ));
    } else {                        // file-matrix test
        TESTING_CHECK( magma_d_csr_mtx( &B,  argv[i], queue ));
    }
    printf("%% matrix info: %lld-by-%lld with %lld nonzeros\n",
            (long long) B.num_rows, (long long) B.num_cols, (long long) B.nnz );


    TESTING_CHECK( magma_dmtransfer( A, &dA, Magma_CPU, Magma_DEV, queue ));
    TESTING_CHECK( magma_dmtransfer( B, &dB, Magma_CPU, Magma_DEV, queue ));

    TESTING_CHECK( magma_dcuspaxpy( &one, dA, &one, dB, &dC, queue ));

    magma_dmfree(&dB, queue );

    TESTING_CHECK( magma_dcuspaxpy( &mone, dA, &one, dC, &dB, queue ));
    
    TESTING_CHECK( magma_dmtransfer( dB, &B2, Magma_DEV, Magma_CPU, queue ));

    magma_dmfree(&dA, queue );
    magma_dmfree(&dB, queue );
    magma_dmfree(&dC, queue );

    // check difference
    TESTING_CHECK( magma_dmdiff( B, B2, &res, queue ));
    printf("%% ||A-B||_F = %8.2e\n", res);
    if ( res < .000001 )
        printf("%% tester matrix add:  ok\n");
    else
        printf("%% tester matrix add:  failed\n");

    magma_dmfree(&A, queue );
    magma_dmfree(&B, queue );
    magma_dmfree(&B2, queue );
    magma_queue_destroy( queue );
    TESTING_CHECK( magma_finalize() );
    return info;
}
