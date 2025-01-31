/*
    -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date
       
       @author Hartwig Anzt
       @author Goran Flegar

       @generated from sparse/blas/zgeisai_batched32.cu, normal z -> c, Sat Mar 27 20:32:37 2021

*/
#include "magmasparse_internal.h"
#include "shuffle.cuh"
#include <cuda_profiler_api.h>


#define PRECISION_c
#define COMPLEX
#define BLOCKSIZE 32
#define WARP_SIZE 32
#define WRP 32
#define WRQ 4

#include <cuda.h>  // for CUDA_VERSION

#if (CUDA_VERSION >= 7000) // only for cuda>6000


const int MaxBlockSize = 32;


template <int block_size>
__device__ void
magma_clowerisai_regs_inv_kernel(
magma_int_t num_rows,
const magma_index_t * __restrict__ Arow,
const magma_index_t * __restrict__ Acol,
const magmaFloatComplex * __restrict__ Aval,
magma_index_t *Mrow,
magma_index_t *Mcol,
magmaFloatComplex *Mval )
{
#if (defined( REAL ) && ( __CUDA_ARCH__ >= 300 ))
    int tid = threadIdx.x;
    int row = gridDim.x*blockIdx.y*blockDim.y + blockIdx.x*blockDim.y + threadIdx.y;

    if( tid >= block_size )
        return;

    if( row >= num_rows )
        return;

    // only if within the size
    int mstart = Mrow[ row ];
    int mlim = Mrow[ row ]-1;

    magmaFloatComplex rB;                // registers for trsv
    magmaFloatComplex dA[ block_size ];  // registers for trisystem
    magmaFloatComplex rA;

    // set dA to 0
    #pragma unroll
    for( int j = 0; j < block_size; j++ ){
        dA[ j ] = MAGMA_C_ZERO;
    }

    // generate the triangular systems
    int t = Mcol[ mstart + tid ];
    int k = Arow[ t+1 ] - 1;
    int alim = Arow[ t ]-1;
    int l = Mrow[ row+1 ]-1;
    int idx = block_size-1;
    while( k > alim && l > mlim  ){ // stop once this column is done
        int mcol =  Mcol[ l ];
        int acol = Acol[k];
        if( mcol == acol ){ //match
            dA[ idx ] = Aval[ k ];
            k--;
            l--;
            idx--;
        } else if( acol > mcol ){// need to check next element
            k--;
        } else { // element does not exist, i.e. l < LC.col[k]
            l--; // check next elment in the sparsity pattern
            idx--; // leave this element equal zero
        }
    }

    // second: solve the triangular systems - in registers
    // we know how RHS looks like
    rB = ( tid == 0 ) ? MAGMA_C_ONE : MAGMA_C_ZERO;

        // Triangular solve in regs.
    #pragma unroll
    for (int k = 0; k < block_size; k++)
    {
        rA = dA[ k ];
        if (k%block_size == tid)
            rB /= rA;
        magmaFloatComplex top = magmablas_cshfl(rB, k%block_size);
        if ( tid > k)
            rB -= (top*rA);
    }

    // Drop B to dev memory - in ISAI preconditioner M
    Mval[ mstart + tid ] = rB;

#endif

}


template <int block_size>
__device__ __forceinline__ void
magma_clowerisai_regs_inv_select(
int N,
magma_int_t num_rows,
const magma_index_t * __restrict__ Arow,
const magma_index_t * __restrict__ Acol,
const magmaFloatComplex * __restrict__ Aval,
magma_index_t *Mrow,
magma_index_t *Mcol,
magmaFloatComplex *Mval )
{
    if (N == block_size) {
        magma_clowerisai_regs_inv_kernel<block_size>(
                num_rows, Arow, Acol, Aval, Mrow, Mcol, Mval);
    } else {
        magma_clowerisai_regs_inv_select<block_size-1>(
                N, num_rows, Arow, Acol, Aval, Mrow, Mcol, Mval);
    }
}


template <>
__device__ __forceinline__ void
magma_clowerisai_regs_inv_select<0>(
int N,
magma_int_t num_rows,
const magma_index_t * __restrict__ Arow,
const magma_index_t * __restrict__ Acol,
const magmaFloatComplex * __restrict__ Aval,
magma_index_t *Mrow,
magma_index_t *Mcol,
magmaFloatComplex *Mval )
{
    ;
    // out of range - do nothing.
    // printf("%% error: size out of range: %d\n", N);
}


__global__ void
magma_clowerisai_regs_inv_switch(
magma_int_t num_rows,
const magma_index_t * __restrict__ Arow,
const magma_index_t * __restrict__ Acol,
const magmaFloatComplex * __restrict__ Aval,
magma_index_t *Mrow,
magma_index_t *Mcol,
magmaFloatComplex *Mval )
{
    int row = gridDim.x*blockIdx.y*blockDim.y + blockIdx.x*blockDim.y + threadIdx.y;
    if( row < num_rows ){
        int N = Mrow[ row+1 ] - Mrow[ row ];
        magma_clowerisai_regs_inv_select<MaxBlockSize>(
                N, num_rows, Arow, Acol, Aval, Mrow, Mcol, Mval);
    }
}


template <int block_size>
__device__ void
magma_cupperisai_regs_inv_kernel(
magma_int_t num_rows,
const magma_index_t * __restrict__ Arow,
const magma_index_t * __restrict__ Acol,
const magmaFloatComplex * __restrict__ Aval,
magma_index_t *Mrow,
magma_index_t *Mcol,
magmaFloatComplex *Mval )
{
#if (defined( REAL ) && ( __CUDA_ARCH__ >= 300 ))
    int tid = threadIdx.x;
    int row = gridDim.x*blockIdx.y*blockDim.y + blockIdx.x*blockDim.y + threadIdx.y;

    if( tid >= block_size )
        return;

    if( row >= num_rows )
        return;

    // only if within the size
    int mstart = Mrow[ row ];
    int mlim = Mrow[ row ]-1;

    magmaFloatComplex rB;                // registers for trsv
    magmaFloatComplex dA[ block_size ];  // registers for trisystem
    magmaFloatComplex rA;

    // set dA to 0
    #pragma unroll
    for( int j = 0; j < block_size; j++ ){
        dA[ j ] = MAGMA_C_ZERO;
    }

    // generate the triangular systems
    int t = Mcol[ mstart + tid ];
    int k = Arow[ t+1 ] - 1;
    int alim = Arow[ t ]-1;
    int l = Mrow[ row+1 ]-1;
    int idx = block_size-1;
    while( k > alim && l > mlim  ){ // stop once this column is done
        int mcol =  Mcol[ l ];
        int acol = Acol[k];
        if( mcol == acol ){ //match
            dA[ idx ] = Aval[ k ];
            k--;
            l--;
            idx--;
        } else if( acol > mcol ){// need to check next element
            k--;
        } else { // element does not exist, i.e. l < LC.col[k]
            l--; // check next elment in the sparsity pattern
            idx--; // leave this element equal zero
        }
    }

    // second: solve the triangular systems - in registers
    // we know how RHS looks like
    rB = ( tid == block_size-1 ) ? MAGMA_C_ONE : MAGMA_C_ZERO;

        // Triangular solve in regs.
    #pragma unroll
    for (int k = block_size-1; k >-1; k--)
    {
        rA = dA[ k ];
        if (k%block_size == tid)
            rB /= rA;
        magmaFloatComplex bottom = magmablas_cshfl(rB, k%block_size);
        if ( tid < k)
            rB -= (bottom*rA);
    }

    // Drop B to dev memory - in ISAI preconditioner M
    Mval[ mstart + tid ] = rB;

#endif
}


template <int block_size>
__device__ __forceinline__ void
magma_cupperisai_regs_inv_select(
int N,
magma_int_t num_rows,
const magma_index_t * __restrict__ Arow,
const magma_index_t * __restrict__ Acol,
const magmaFloatComplex * __restrict__ Aval,
magma_index_t *Mrow,
magma_index_t *Mcol,
magmaFloatComplex *Mval )
{
    if (N == block_size) {
        magma_cupperisai_regs_inv_kernel<block_size>(
                num_rows, Arow, Acol, Aval, Mrow, Mcol, Mval);
    } else {
        magma_cupperisai_regs_inv_select<block_size-1>(
                N, num_rows, Arow, Acol, Aval, Mrow, Mcol, Mval);
    }
}


template <>
__device__ __forceinline__ void
magma_cupperisai_regs_inv_select<0>(
int N,
magma_int_t num_rows,
const magma_index_t * __restrict__ Arow,
const magma_index_t * __restrict__ Acol,
const magmaFloatComplex * __restrict__ Aval,
magma_index_t *Mrow,
magma_index_t *Mcol,
magmaFloatComplex *Mval )
{
    ;
    // out of range - do nothing.
    // printf("%% error: size out of range: %d\n", N);
}


__global__ void
magma_cupperisai_regs_inv_switch(
magma_int_t num_rows,
const magma_index_t * __restrict__ Arow,
const magma_index_t * __restrict__ Acol,
const magmaFloatComplex * __restrict__ Aval,
magma_index_t *Mrow,
magma_index_t *Mcol,
magmaFloatComplex *Mval )
{
    int row = gridDim.x*blockIdx.y*blockDim.y + blockIdx.x*blockDim.y + threadIdx.y;
    if( row < num_rows ){
        int N = Mrow[ row+1 ] - Mrow[ row ];
        magma_cupperisai_regs_inv_select<MaxBlockSize>(
                N, num_rows, Arow, Acol, Aval, Mrow, Mcol, Mval);
    }
}

#endif


/**
    Purpose
    -------
    This routine is designet to combine all kernels into one.

    Arguments
    ---------


    @param[in]
    uplotype    magma_uplo_t
                lower or upper triangular

    @param[in]
    transtype   magma_trans_t
                possibility for transposed matrix

    @param[in]
    diagtype    magma_diag_t
                unit diagonal or not

    @param[in]
    L           magma_c_matrix
                triangular factor for which the ISAI matrix is computed.
                Col-Major CSR storage.

    @param[in,out]
    M           magma_c_matrix*
                SPAI preconditioner CSR col-major

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_caux
    ********************************************************************/

extern "C" magma_int_t
magma_cisai_generator_regs(
    magma_uplo_t uplotype,
    magma_trans_t transtype,
    magma_diag_t diagtype,
    magma_c_matrix L,
    magma_c_matrix *M,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    

#if (CUDA_VERSION >= 7000)
    magma_int_t arch = magma_getdevice_arch();

    cudaDeviceSetCacheConfig( cudaFuncCachePreferL1 );


    int r2bs1 = 32;
    int r2bs2 = 4;
    int necessary_blocks = magma_ceildiv(L.num_rows, r2bs2);
    int r2dg1 = min( int( sqrt( float( necessary_blocks ))), 65535 );
    int r2dg2 = min(magma_ceildiv( necessary_blocks, r2dg1 ), 65535);
    int r2dg3 = magma_ceildiv( necessary_blocks, r2dg1*r2dg2 );
    dim3 r2block( r2bs1, r2bs2, 1 );
    dim3 r2grid( r2dg1, r2dg2, r2dg3 );


    if (arch >= 300) {
        if (uplotype == MagmaLower) { //printf("in here lower new kernel\n");
            magma_clowerisai_regs_inv_switch<<< r2grid, r2block, 0, queue->cuda_stream() >>>(
                L.num_rows,
                L.row,
                L.col,
                L.val,
                M->row,
                M->col,
                M->val );
        }
        else { // printf("in here upper new kernel\n");
            magma_cupperisai_regs_inv_switch<<< r2grid, r2block, 0, queue->cuda_stream() >>>(
                L.num_rows,
                L.row,
                L.col,
                L.val,
                M->row,
                M->col,
                M->val );
        }
    }
    else {
       printf( "%% error: ISAI preconditioner requires CUDA ARCHITECTURE >= 300.\n" );
       info = MAGMA_ERR_NOT_SUPPORTED;
    }
#else
    // CUDA < 7000
    printf( "%% error: ISAI preconditioner requires CUDA >= 7.0.\n" );
    info = MAGMA_ERR_NOT_SUPPORTED;
#endif

    return info;
}
