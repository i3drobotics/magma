/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/blas/zparilu_kernels.cu, normal z -> s, Sat Mar 27 20:32:32 2021

*/
#include "magmasparse_internal.h"

#define PRECISION_s


__global__ void 
magma_sparilu_csr_kernel(  
    const magma_int_t num_rows, 
    const magma_int_t nnz,  
    const magma_index_t *rowidxA, 
    const magma_index_t *colidxA,
    const float * __restrict__ A, 
    const magma_index_t *rowptrL, 
    const magma_index_t *colidxL, 
    float *valL, 
    const magma_index_t *rowptrU, 
    const magma_index_t *colidxU, 
    float *valU)
{
    int i, j;
    int k = blockDim.x * blockIdx.x + threadIdx.x;
    float zero = MAGMA_S_MAKE(0.0, 0.0);
    float s, sp;
    int il, iu, jl, ju;
    if (k < nnz) {
       i = rowidxA[k];
        j = colidxA[k];
#if (__CUDA_ARCH__ >= 350) && (defined(PRECISION_d) || defined(PRECISION_s))
        s =  __ldg(A+k);
#else
        s =  A[k];
#endif
        il = rowptrL[i];
        iu = rowptrU[j];

        while (il < rowptrL[i+1] && iu < rowptrU[j+1]) {
            sp = zero;
            jl = colidxL[il];
            ju = colidxU[iu];
            sp = (jl == ju) ? valL[il] * valU[iu] : sp;
            s = (jl == ju) ? s-sp : s;
            il = (jl <= ju) ? il+1 : il;
            iu = (jl >= ju) ? iu+1 : iu;
        }
        s += sp;  // undo the last operation (it must be the last)        
        if (i > j)      // modify l entry
            valL[il-1] =  s / valU[rowptrU[j+1]-1];
        else            // modify u entry
            valU[iu-1] = s;
    }
}




/**
    Purpose
    -------
    
    This routine iteratively computes an incomplete LU factorization.
    For reference, see:
    E. Chow and A. Patel: "Fine-grained Parallel Incomplete LU Factorization", 
    SIAM Journal on Scientific Computing, 37, C169-C193 (2015). 
    This routine was used in the ISC 2015 paper:
    E. Chow et al.: "Asynchronous Iterative Algorithm for Computing Incomplete
                     Factorizations on GPUs", 
                     ISC High Performance 2015, LNCS 9137, pp. 1-16, 2015.
 
    The input format of the system matrix is COO, the lower triangular factor L 
    is stored in CSR, the upper triangular factor U is transposed, then also 
    stored in CSR (equivalent to CSC format for the non-transposed U).
    Every component of L and U is handled by one thread. 

    Arguments
    ---------

    @param[in]
    A           magma_s_matrix
                input matrix A determing initial guess & processing order

    @param[in,out]
    L           magma_s_matrix
                input/output matrix L containing the lower triangular factor 

    @param[in,out]
    U           magma_s_matrix
                input/output matrix U containing the upper triangular factor
                              
    @param[in]
    queue       magma_queue_t
                Queue to execute in.
                
    @ingroup magmasparse_sgegpuk
    ********************************************************************/

extern "C" magma_int_t
magma_sparilu_csr(
    magma_s_matrix A,
    magma_s_matrix L,
    magma_s_matrix U,
    magma_queue_t queue)
{
    int blocksize1 = 128;
    int blocksize2 = 1;

    int dimgrid1 = magma_ceildiv(A.nnz, blocksize1);
    int dimgrid2 = 1;
    int dimgrid3 = 1;
    dim3 grid(dimgrid1, dimgrid2, dimgrid3);
    dim3 block(blocksize1, blocksize2, 1);
    magma_sparilu_csr_kernel<<< grid, block, 0, queue->cuda_stream() >>>
        (A.num_rows, A.nnz, A.rowidx, A.col, A.val, 
         L.row, L.col, L.val, 
         U.row, U.col, U.val);

    return MAGMA_SUCCESS;
}
