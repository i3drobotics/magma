/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Hartwig Anzt

       @generated from sparse/src/zparic_cpu.cpp, normal z -> c, Sat Mar 27 20:33:03 2021
*/

#include "magmasparse_internal.h"
#ifdef _OPENMP
#include <omp.h>
#endif

#define PRECISION_c


/***************************************************************************//**
    Purpose
    -------

    Generates an IC(0) preconditer via fixed-point iterations. 
    For reference, see:
    E. Chow and A. Patel: "Fine-grained Parallel Incomplete LU Factorization", 
    SIAM Journal on Scientific Computing, 37, C169-C193 (2015). 
    
    This is the CPU implementation of the ParIC

    Arguments
    ---------

    @param[in]
    A           magma_c_matrix
                input matrix A

    @param[in]
    b           magma_c_matrix
                input RHS b

    @param[in,out]
    precond     magma_c_preconditioner*
                preconditioner parameters

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_cgepr
*******************************************************************************/
extern "C"
magma_int_t
magma_cparic_cpu(
    magma_c_matrix A,
    magma_c_matrix b,
    magma_c_preconditioner *precond,
    magma_queue_t queue)
{
    magma_int_t info = MAGMA_ERR_NOT_SUPPORTED;
    
#ifdef _OPENMP
    info = 0;

    magma_c_matrix hAT={Magma_CSR}, hA={Magma_CSR}, hAL={Magma_CSR}, 
    hAU={Magma_CSR}, hAUT={Magma_CSR}, hAtmp={Magma_CSR}, hACOO={Magma_CSR};

    // copy original matrix as COO to device
    if (A.memory_location != Magma_CPU || A.storage_type != Magma_CSR) {
        CHECK(magma_cmtransfer(A, &hAT, A.memory_location, Magma_CPU, queue));
        CHECK(magma_cmconvert(hAT, &hA, hAT.storage_type, Magma_CSR, queue));
        magma_cmfree(&hAT, queue);
    } else {
        CHECK(magma_cmtransfer(A, &hA, A.memory_location, Magma_CPU, queue));
    }

    // in case using fill-in
    if (precond->levels > 0) {
        CHECK(magma_csymbilu(&hA, precond->levels, &hAL, &hAUT,  queue));
        magma_cmfree(&hAL, queue);
        magma_cmfree(&hAUT, queue);
    }
    CHECK(magma_cmconvert(hA, &hACOO, hA.storage_type, Magma_CSRCOO, queue));
    
    //get L
    magma_cmatrix_tril(hA, &hAL, queue);
    
    CHECK(magma_cmconvert(hAL, &hACOO, hA.storage_type, Magma_CSRCOO, queue));
    
    
    // This is the actual ParIC kernel. 
    // It can be called directly if
    // - the system matrix hALCOO is available in COO format on the CPU 
    // - hAL is the lower triangular in CSR on the CPU
    // The kernel is located in sparse/control/magma_cparic_kernels.cpp
    //
    for (int i=0; i<precond->sweeps; i++) {
        CHECK(magma_cparic_sweep(hACOO, &hAL, queue));
    }
    

    CHECK(magma_cmtransfer(hAL, &precond->L, Magma_CPU, Magma_DEV, queue));
    CHECK(magma_c_cucsrtranspose(precond->L, &precond->U, queue));
    CHECK(magma_cmtransfer(precond->L, &precond->M, Magma_DEV, Magma_DEV, queue));
    
    if (precond->trisolver == 0 || precond->trisolver == Magma_CUSOLVE) {
        CHECK(magma_ccumicgeneratesolverinfo(precond, queue));
    } else {
        //prepare for iterative solves

        // extract the diagonal of L into precond->d
        CHECK(magma_cjacobisetup_diagscal(precond->L, &precond->d, queue));
        CHECK(magma_cvinit(&precond->work1, Magma_DEV, hA.num_rows, 1, 
            MAGMA_C_ZERO, queue));

        // extract the diagonal of U into precond->d2
        CHECK(magma_cjacobisetup_diagscal(precond->U, &precond->d2, queue));
        CHECK(magma_cvinit(&precond->work2, Magma_DEV, hA.num_rows, 1, 
            MAGMA_C_ZERO, queue));
    }

cleanup:
    magma_cmfree(&hAT, queue);
    magma_cmfree(&hA, queue);
    magma_cmfree(&hAL, queue);
    magma_cmfree(&hAU, queue);
    magma_cmfree(&hAUT, queue);
    magma_cmfree(&hAtmp, queue);
    magma_cmfree(&hACOO, queue);

#endif
    return info;
}
