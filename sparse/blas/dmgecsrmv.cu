/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/blas/zmgecsrmv.cu, normal z -> d, Sat Mar 27 20:32:29 2021

*/
#include "magmasparse_internal.h"

#define BLOCK_SIZE 512


__global__ void 
dmgecsrmv_kernel( 
    int num_rows, 
    int num_cols, 
    int num_vecs,
    double alpha, 
    double * dval, 
    magma_index_t * drowptr, 
    magma_index_t * dcolind,
    double * dx,
    double beta, 
    double * dy)
{
    int row = blockIdx.x*blockDim.x+threadIdx.x;
    int j;
    extern __shared__ double dot[];

    if( row<num_rows ){
        for( int i=0; i<num_vecs; i++ )
                dot[ threadIdx.x+ i*blockDim.x ] = MAGMA_D_MAKE(0.0, 0.0);
        int start = drowptr[ row ];
        int end = drowptr[ row+1 ];
        for( j=start; j<end; j++ ){
            int col = dcolind [ j ];
            double val = dval[ j ];
            for( int i=0; i<num_vecs; i++ )
                dot[ threadIdx.x + i*blockDim.x ] += 
                                    val * dx[ col + i*num_cols ];
        }
        for( int i=0; i<num_vecs; i++ )
            dy[ row +i*num_cols ] = alpha * dot[ threadIdx.x + i*blockDim.x ] 
                                             + beta * dy[ row + i*num_cols ];
    }
}



/**
    Purpose
    -------
    
    This routine computes Y = alpha *  A *  X + beta * Y for X and Y sets of 
    num_vec vectors on the GPU. Input format is CSR. 
    
    Arguments
    ---------
    
    @param[in]
    transA      magma_trans_t
                transposition parameter for A

    @param[in]
    m           magma_int_t
                number of rows in A

    @param[in]
    n           magma_int_t
                number of columns in A 
                
    @param[in]
    num_vecs    mama_int_t
                number of vectors
    @param[in]
    alpha       double
                scalar multiplier

    @param[in]
    dval        magmaDouble_ptr
                array containing values of A in CSR

    @param[in]
    drowptr     magmaIndex_ptr
                rowpointer of A in CSR

    @param[in]
    dcolind     magmaIndex_ptr
                columnindices of A in CSR

    @param[in]
    dx          magmaDouble_ptr
                input vector x

    @param[in]
    beta        double
                scalar multiplier

    @param[out]
    dy          magmaDouble_ptr
                input/output vector y

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_dblas
    ********************************************************************/

extern "C" magma_int_t
magma_dmgecsrmv(
    magma_trans_t transA,
    magma_int_t m, magma_int_t n,
    magma_int_t num_vecs, 
    double alpha,
    magmaDouble_ptr dval,
    magmaIndex_ptr drowptr,
    magmaIndex_ptr dcolind,
    magmaDouble_ptr dx,
    double beta,
    magmaDouble_ptr dy,
    magma_queue_t queue )
{
    dim3 grid( magma_ceildiv( m, BLOCK_SIZE ), 1, 1);
    magma_int_t threads = BLOCK_SIZE;
    unsigned int MEM_SIZE =  num_vecs* BLOCK_SIZE 
                    * sizeof( double ); // num_vecs vectors 
    dmgecsrmv_kernel<<< grid, threads, MEM_SIZE, queue->cuda_stream()>>>
            (m, n, num_vecs, alpha, dval, drowptr, dcolind, dx, beta, dy);

    return MAGMA_SUCCESS;
}
