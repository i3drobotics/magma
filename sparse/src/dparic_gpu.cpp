/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Hartwig Anzt

       @generated from sparse/src/zparic_gpu.cpp, normal z -> d, Sat Mar 27 20:33:02 2021
*/

#include "magmasparse_internal.h"
#ifdef _OPENMP
#include <omp.h>
#endif

#define PRECISION_d


/***************************************************************************//**
    Purpose
    -------

    Generates an IC(0) preconditer via fixed-point iterations. 
    For reference, see:
    E. Chow and A. Patel: "Fine-grained Parallel Incomplete LU Factorization", 
    SIAM Journal on Scientific Computing, 37, C169-C193 (2015). 
    
    This is the GPU implementation of the ParIC

    Arguments
    ---------

    @param[in]
    A           magma_d_matrix
                input matrix A

    @param[in]
    b           magma_d_matrix
                input RHS b

    @param[in,out]
    precond     magma_d_preconditioner*
                preconditioner parameters

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_dgepr
*******************************************************************************/
extern "C"
magma_int_t
magma_dparic_gpu(
    magma_d_matrix A,
    magma_d_matrix b,
    magma_d_preconditioner *precond,
    magma_queue_t queue)
{
    magma_int_t info = MAGMA_ERR_NOT_SUPPORTED;
    
#ifdef _OPENMP
    info = 0;

    magma_d_matrix hAT={Magma_CSR}, hA={Magma_CSR}, hAL={Magma_CSR}, 
    hAU={Magma_CSR}, hAUT={Magma_CSR}, hAtmp={Magma_CSR}, hACOO={Magma_CSR},
    dAL={Magma_CSR}, dACOO={Magma_CSR};

    // copy original matrix as COO to device
    if (A.memory_location != Magma_CPU || A.storage_type != Magma_CSR) {
        CHECK(magma_dmtransfer(A, &hAT, A.memory_location, Magma_CPU, queue));
        CHECK(magma_dmconvert(hAT, &hA, hAT.storage_type, Magma_CSR, queue));
        magma_dmfree(&hAT, queue);
    } else {
        CHECK(magma_dmtransfer(A, &hA, A.memory_location, Magma_CPU, queue));
    }

    // in case using fill-in
    if (precond->levels > 0) {
        CHECK(magma_dsymbilu(&hA, precond->levels, &hAL, &hAUT,  queue));
        magma_dmfree(&hAL, queue);
        magma_dmfree(&hAUT, queue);
    }
    CHECK(magma_dmconvert(hA, &hACOO, hA.storage_type, Magma_CSRCOO, queue));
    
    //get L
    magma_dmatrix_tril(hA, &hAL, queue);
    
    CHECK(magma_dmconvert(hAL, &hACOO, hA.storage_type, Magma_CSRCOO, queue));
    CHECK(magma_dmtransfer(hAL, &dAL, Magma_CPU, Magma_DEV, queue));
    CHECK(magma_dmtransfer(hACOO, &dACOO, Magma_CPU, Magma_DEV, queue));
    
    // This is the actual ParIC kernel. 
    // It can be called directly if
    // - the system matrix hALCOO is available in COO format on the GPU 
    // - hAL is the lower triangular in CSR on the GPU
    // The kernel is located in sparse/blas/dparic_kernels.cu
    //
    for (int i=0; i<precond->sweeps; i++) {
        CHECK(magma_dparic_csr(dACOO, dAL, queue));
    }
    

    CHECK(magma_dmtransfer(dAL, &precond->L, Magma_DEV, Magma_DEV, queue));
    CHECK(magma_d_cucsrtranspose(precond->L, &precond->U, queue));
    CHECK(magma_dmtransfer(precond->L, &precond->M, Magma_DEV, Magma_DEV, queue));
    
    if (precond->trisolver == 0 || precond->trisolver == Magma_CUSOLVE) {
        CHECK(magma_dcumicgeneratesolverinfo(precond, queue));
    } else {
        //prepare for iterative solves

        // extract the diagonal of L into precond->d
        CHECK(magma_djacobisetup_diagscal(precond->L, &precond->d, queue));
        CHECK(magma_dvinit(&precond->work1, Magma_DEV, hA.num_rows, 1, 
            MAGMA_D_ZERO, queue));

        // extract the diagonal of U into precond->d2
        CHECK(magma_djacobisetup_diagscal(precond->U, &precond->d2, queue));
        CHECK(magma_dvinit(&precond->work2, Magma_DEV, hA.num_rows, 1, 
            MAGMA_D_ZERO, queue));
    }

cleanup:
    magma_dmfree(&hAT, queue);
    magma_dmfree(&hA, queue);
    magma_dmfree(&hAL, queue);
    magma_dmfree(&dAL, queue);
    magma_dmfree(&hAU, queue);
    magma_dmfree(&hAUT, queue);
    magma_dmfree(&hAtmp, queue);
    magma_dmfree(&hACOO, queue);
    magma_dmfree(&dACOO, queue);

#endif
    return info;
}
