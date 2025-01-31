/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/testing/testing_zmconverter.cpp, normal z -> s, Sat Mar 27 20:33:10 2021
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

    magma_sopts zopts;
    magma_queue_t queue=NULL;
    magma_queue_create( 0, &queue );

    real_Double_t res;
    magma_s_matrix Z={Magma_CSR}, Z2={Magma_CSR}, A={Magma_CSR}, A2={Magma_CSR}, 
    AT={Magma_CSR}, AT2={Magma_CSR}, B={Magma_CSR};
    int i=1;
    TESTING_CHECK( magma_sparse_opts( argc, argv, &zopts, &i, queue ));

    B.blocksize = zopts.blocksize;
    B.alignment = zopts.alignment;

    while( i < argc ) {
        if ( strcmp("LAPLACE2D", argv[i]) == 0 && i+1 < argc ) {   // Laplace test
            i++;
            magma_int_t laplace_size = atoi( argv[i] );
            TESTING_CHECK( magma_sm_5stencil(  laplace_size, &Z, queue ));
        } else {                        // file-matrix test
            TESTING_CHECK( magma_s_csr_mtx( &Z,  argv[i], queue ));
        }

        printf("%% matrix info: %lld-by-%lld with %lld nonzeros\n",
                (long long) Z.num_rows, (long long) Z.num_cols, (long long) Z.nnz );
        
        // convert to be non-symmetric
        TESTING_CHECK( magma_smconvert( Z, &A, Magma_CSR, Magma_CSRL, queue ));
        TESTING_CHECK( magma_smconvert( Z, &B, Magma_CSR, Magma_CSRU, queue ));

        // transpose
        TESTING_CHECK( magma_smtranspose( A, &AT, queue ));

        // quite some conversions
                    
        //ELL
        TESTING_CHECK( magma_smconvert( AT, &AT2, Magma_CSR, Magma_ELL, queue ));
        magma_smfree(&AT, queue );
        TESTING_CHECK( magma_smconvert( AT2, &AT, Magma_ELL, Magma_CSR, queue ));
        magma_smfree(&AT2, queue );
        //ELLPACKT
        TESTING_CHECK( magma_smconvert( AT, &AT2, Magma_CSR, Magma_ELLPACKT, queue ));
        magma_smfree(&AT, queue );
        TESTING_CHECK( magma_smconvert( AT2, &AT, Magma_ELLPACKT, Magma_CSR, queue ));
        magma_smfree(&AT2, queue );
        //ELLRT
        AT2.blocksize = 8;
        AT2.alignment = 8;
        TESTING_CHECK( magma_smconvert( AT, &AT2, Magma_CSR, Magma_ELLRT, queue ));
        magma_smfree(&AT, queue );
        TESTING_CHECK( magma_smconvert( AT2, &AT, Magma_ELLRT, Magma_CSR, queue ));
        magma_smfree(&AT2, queue );
        //SELLP
        AT2.blocksize = 8;
        AT2.alignment = 8;
        TESTING_CHECK( magma_smconvert( AT, &AT2, Magma_CSR, Magma_SELLP, queue ));
        magma_smfree(&AT, queue );
        TESTING_CHECK( magma_smconvert( AT2, &AT, Magma_SELLP, Magma_CSR, queue ));
        magma_smfree(&AT2, queue );
        //ELLD
        TESTING_CHECK( magma_smconvert( AT, &AT2, Magma_CSR, Magma_ELLD, queue ));
        magma_smfree(&AT, queue );
        TESTING_CHECK( magma_smconvert( AT2, &AT, Magma_ELLD, Magma_CSR, queue ));
        magma_smfree(&AT2, queue );
        //CSRCOO
        TESTING_CHECK( magma_smconvert( AT, &AT2, Magma_CSR, Magma_CSRCOO, queue ));
        magma_smfree(&AT, queue );
        TESTING_CHECK( magma_smconvert( AT2, &AT, Magma_CSRCOO, Magma_CSR, queue ));
        magma_smfree(&AT2, queue );
        //CSRLIST
        TESTING_CHECK( magma_smconvert( AT, &AT2, Magma_CSR, Magma_CSRLIST, queue ));
        magma_smfree(&AT, queue );
        TESTING_CHECK( magma_smconvert( AT2, &AT, Magma_CSRLIST, Magma_CSR, queue ));
        magma_smfree(&AT2, queue );
        //CSRD
        TESTING_CHECK( magma_smconvert( AT, &AT2, Magma_CSR, Magma_CSRD, queue ));
        magma_smfree(&AT, queue );
        TESTING_CHECK( magma_smconvert( AT2, &AT, Magma_CSRD, Magma_CSR, queue ));
        magma_smfree(&AT2, queue );
        
        // transpose
        TESTING_CHECK( magma_smtranspose( AT, &A2, queue ));
        TESTING_CHECK( magma_smdiff( A, A2, &res, queue));
        printf("%% ||A-A2||_F = %8.2e\n", res);
        if ( res < .000001 )
            printf("%% conversion tester:  ok\n");
        else
            printf("%% conversion tester:  failed\n");
        
        TESTING_CHECK( magma_smlumerge( A2, B, &Z2, queue ));

        TESTING_CHECK( magma_smdiff( Z, Z2, &res, queue));        
        printf("%% ||Z-Z2||_F = %8.2e\n", res);
        if ( res < .000001 )
            printf("%% LUmerge tester:  ok\n");
        else
            printf("%% LUmerge tester:  failed\n");

        magma_smfree(&A, queue );
        magma_smfree(&A2, queue );
        magma_smfree(&AT, queue );
        magma_smfree(&AT2, queue );
        magma_smfree(&B, queue );
        magma_smfree(&Z2, queue );
        magma_smfree(&Z, queue );

        i++;
    }

    magma_queue_destroy( queue );
    TESTING_CHECK( magma_finalize() );
    return info;
}
