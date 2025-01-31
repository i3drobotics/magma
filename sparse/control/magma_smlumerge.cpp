/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/control/magma_zmlumerge.cpp, normal z -> s, Sat Mar 27 20:32:43 2021
       @author Hartwig Anzt

*/
#include "magmasparse_internal.h"

#define RTOLERANCE     lapackf77_slamch( "E" )
#define ATOLERANCE     lapackf77_slamch( "E" )


/**
    Purpose
    -------

    Takes an strictly lower triangular matrix L and an upper triangular matrix U
    and merges them into a matrix A containing the upper and lower triangular
    parts.

    Arguments
    ---------

    @param[in]
    L           magma_s_matrix
                input strictly lower triangular matrix L

    @param[in]
    U           magma_s_matrix
                input upper triangular matrix U
    
    @param[out]
    A           magma_s_matrix*
                output matrix
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_saux
    ********************************************************************/

extern "C" magma_int_t
magma_smlumerge(
    magma_s_matrix L,
    magma_s_matrix U,
    magma_s_matrix *A,
    magma_queue_t queue )
{
    magma_int_t info = 0;    
    
    // make sure the target structure is empty
    magma_smfree( A, queue );

    if( L.storage_type == Magma_CSR && U.storage_type == Magma_CSR ){
        if( L.memory_location == Magma_CPU && U.memory_location == Magma_CPU ){
            CHECK( magma_smtransfer( L, A, Magma_CPU, Magma_CPU, queue ));
            magma_free_cpu( A->col );
            magma_free_cpu( A->val );
            // make sure it is strictly lower triangular
            magma_int_t z = 0;
            for(magma_int_t i=0; i<A->num_rows; i++){
                for(magma_int_t j=L.row[i]; j<L.row[i+1]; j++){
                    if( L.col[j] < i ){// make sure it is strictly lower triangular
                        z++;
                    }
                }
                for(magma_int_t j=U.row[i]; j<U.row[i+1]; j++){
                    z++;
                }
            }
            A->nnz = z;
            // fill A with the new structure;
            CHECK( magma_index_malloc_cpu( &A->col, A->nnz ));
            CHECK( magma_smalloc_cpu( &A->val, A->nnz ));
            z = 0;
            for(magma_int_t i=0; i<A->num_rows; i++){
                A->row[i] = z;
                for(magma_int_t j=L.row[i]; j<L.row[i+1]; j++){
                    if( L.col[j] < i ){// make sure it is strictly lower triangular
                        A->col[z] = L.col[j];
                        A->val[z] = L.val[j];
                        z++;
                    }
                }
                for(magma_int_t j=U.row[i]; j<U.row[i+1]; j++){
                    A->col[z] = U.col[j];
                    A->val[z] = U.val[j];
                    z++;
                }
            }
            A->row[A->num_rows] = z;
            A->nnz = z;
        }
        else {
            printf("error: matrix not on CPU.\n"); 
            info = MAGMA_ERR_NOT_SUPPORTED;
        }
    }
    else {
            printf("error: matrix in wrong format.\n"); 
            info = MAGMA_ERR_NOT_SUPPORTED;
    }
cleanup:
    if( info != 0 ){
        magma_smfree( A, queue );
    }
    return info;
}
