/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/blas/magma_zmcsrcompressor_gpu.cu, normal z -> c, Sat Mar 27 20:32:34 2021
       @author Hartwig Anzt

*/
#include "magmasparse_internal.h"

#define BLOCK_SIZE1 256
#define BLOCK_SIZE2 1


// copy nonzeros into new structure
__global__ void
magma_cmcsrgpu_kernel1( int num_rows,
                 magmaFloatComplex *A_val,
                 magma_index_t *A_rowptr,
                 magma_index_t *A_colind,
                 magmaFloatComplex *B_val,
                 magma_index_t *B_rowptr,
                 magma_index_t *B_colind )
{
    int row = blockIdx.x*blockDim.x+threadIdx.x;
    int j;

    if(row<num_rows){
        magmaFloatComplex zero = MAGMA_C_ZERO;
        int start = A_rowptr[ row ];
        int new_location = start;
        int end = A_rowptr[ row+1 ];
        for( j=start; j<end; j++ ){
            if( A_val[j] != zero ){
       //         B_val[new_location] = A_val[j];
       //         B_colind[new_location] = A_colind[j];
                new_location++;
            }
        }
        // this is not a correctr rowpointer! this is nn_z in this row!
        B_rowptr[ row ] = new_location-start;
    }
}


// generate a valid rowpointer
__global__ void
magma_cmcsrgpu_kernel2( int num_rows,
                 magma_index_t *B_rowptr,
                 magma_index_t *A_rowptr )
{
    int idx = blockIdx.x*blockDim.x+threadIdx.x;
    int j, nnz = 0;

    if( idx == 0 ){
    A_rowptr[ 0 ] = nnz;
        for( j=0; j<num_rows; j++ ){
            nnz+=B_rowptr[ j ];
            A_rowptr[ j+1 ] = nnz;
        }
    }
}



// copy new structure into original matrix
__global__ void
magma_cmcsrgpu_kernel3( int num_rows,
                 magmaFloatComplex *B_val,
                 magma_index_t *B_rowptr,
                 magma_index_t *B_colind,
                 magma_index_t *B2_rowptr,
                 magmaFloatComplex *A_val,
                 magma_index_t *A_rowptr,
                 magma_index_t *A_colind
                                            )
{
    int row = blockIdx.x*blockDim.x+threadIdx.x;
    int j, new_location;
    
    if(row<num_rows){
    new_location = A_rowptr[ row ];
        int start = B2_rowptr[ row ];
        int end = B2_rowptr[ row+1 ];
        magmaFloatComplex zero = MAGMA_C_ZERO;
        for( j=start; j<end; j++ ){
            if( A_val[j] != zero ){
                B_val[new_location] = A_val[j];
                B_colind[new_location] = A_colind[j];
                new_location++;
            }
               // A_val[ j ] = B_val[ j ];
               // A_colind[ j ] = B_colind[ j ];
        }
    }
}


/**
    Purpose
    -------

    Removes zeros in a CSR matrix. This is a GPU implementation of the
    CSR compressor.

    Arguments
    ---------

    @param[in,out]
    A           magma_c_matrix*
                input/output matrix
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_caux
    ********************************************************************/

extern "C" magma_int_t
magma_cmcsrcompressor_gpu(
    magma_c_matrix *A,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    magma_c_matrix B={Magma_CSR}, B2={Magma_CSR};
    magma_c_matrix dA={Magma_CSR}, CSRA={Magma_CSR};
    magma_index_t *cputmp = NULL;
    
    if ( A->memory_location == Magma_DEV && A->storage_type == Magma_CSR ) {
        CHECK( magma_index_malloc( &B.drow, A->num_rows + 1 ));
        CHECK( magma_index_malloc( &B2.drow, A->num_rows + 1 ));
        
        magma_index_copyvector( (A->num_rows+1), A->drow, 1, B2.drow, 1, queue );

        dim3 grid1( magma_ceildiv( A->num_rows, BLOCK_SIZE1 ) );

        // copying the nonzeros into B and write in B.drow how many there are
        magma_cmcsrgpu_kernel1<<< grid1, BLOCK_SIZE1, 0, queue->cuda_stream() >>>
                ( A->num_rows, A->dval, A->drow, A->dcol, B.dval, B.drow, B.dcol );

        // correct the row pointer
        dim3 grid2( 1, 1, 1);
        magma_cmcsrgpu_kernel2<<< grid2, BLOCK_SIZE2, 0, queue->cuda_stream() >>>
                ( A->num_rows, B.drow, A->drow );
        // access the true number of nonzeros

        CHECK( magma_index_malloc_cpu( &cputmp, 1 ));

        magma_index_getvector( 1, A->row+(A->num_rows), 1, cputmp, 1, queue );
        A->nnz = (magma_int_t) cputmp[0];

        // reallocate with right size
        CHECK( magma_cmalloc( &B.dval, A->nnz ));
        CHECK( magma_index_malloc( &B.dcol, A->nnz ));
        
        // copy correct values back
        magma_cmcsrgpu_kernel3<<< grid1, BLOCK_SIZE1, 0, queue->cuda_stream() >>>
                ( A->num_rows, B.dval, B.drow, B.dcol, B2.drow, A->dval, A->drow, A->dcol );

        magma_free( A->dcol );
        magma_free( A->dval );

        A->dcol = B.dcol;
        A->dval = B.dval;
    }
    else {
        magma_storage_t A_storage = A->storage_type;
        magma_location_t A_location = A->memory_location;
        CHECK( magma_cmconvert( *A, &CSRA, A->storage_type, Magma_CSR, queue ));
        CHECK( magma_cmtransfer( *A, &dA, A->memory_location, Magma_DEV, queue ));

        CHECK( magma_cmcsrcompressor_gpu( &dA, queue ));

        magma_cmfree( &dA, queue );
        magma_cmfree( A, queue );
        CHECK( magma_cmtransfer( dA, &CSRA, Magma_DEV, A_location, queue ));
        CHECK( magma_cmconvert( CSRA, A, Magma_CSR, A_storage, queue ));
        magma_cmfree( &dA, queue );
        magma_cmfree( &CSRA, queue );
    }
    
cleanup:
    magma_cmfree( &dA, queue );
    magma_cmfree( &CSRA, queue );
    magma_free( B2.drow );
    magma_free( B.drow );
    return info;
}
