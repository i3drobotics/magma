/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Hartwig Anzt

       @generated from sparse/src/zparict_cpu.cpp, normal z -> s, Sat Mar 27 20:33:04 2021
*/

#include "magmasparse_internal.h"
#ifdef _OPENMP
#include <omp.h>
#endif

#define PRECISION_s


/***************************************************************************//**
    Purpose
    -------

    Generates an incomplete threshold Cholesky preconditioner via the ParILUT 
    algorithm. The strategy is to interleave a parallel fixed-point 
    iteration that approximates an incomplete factorization for a given nonzero 
    pattern with a procedure that adaptively changes the pattern. 
    Much of this algorithm has fine-grained parallelism, and can efficiently 
    exploit the compute power of shared memory architectures.

    This is the routine used in the publication by Anzt, Chow, Dongarra:
    ''ParILUT - A new parallel threshold ILU factorization''
    submitted to SIAM SISC in 2017.
    
    This version uses the default setting which adds all candidates to the
    sparsity pattern. It is the variant for SPD systems.

    This function requires OpenMP, and is only available if OpenMP is activated.
    
    The parameter list is:
    
    precond.sweeps : number of ParILUT steps
    precond.atol   : absolute fill ratio (1.0 keeps nnz count constant)

    Arguments
    ---------

    @param[in]
    A           magma_s_matrix
                input matrix A

    @param[in]
    b           magma_s_matrix
                input RHS b

    @param[in,out]
    precond     magma_s_preconditioner*
                preconditioner parameters

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_sgepr
*******************************************************************************/
extern "C"
magma_int_t
magma_sparict_cpu(
    magma_s_matrix A,
    magma_s_matrix b,
    magma_s_preconditioner *precond,
    magma_queue_t queue)
{
    magma_int_t info = 0;
    
#ifdef _OPENMP

    real_Double_t start, end;
    real_Double_t t_rm=0.0, t_add=0.0, t_res=0.0, t_sweep1=0.0, t_sweep2=0.0, 
        t_cand=0.0, t_transpose1=0.0, t_transpose2=0.0, t_selectrm=0.0,
        t_selectadd=0.0, t_nrm=0.0, t_total = 0.0, accum=0.0;
                    
    float sum, sumL;

    magma_s_matrix hA={Magma_CSR},
        hL={Magma_CSR}, oneL={Magma_CSR}, LT={Magma_CSR},
        L={Magma_CSR}, L_new={Magma_CSR}, L0={Magma_CSR};  
    magma_int_t num_rmL;
    float thrsL = 0.0;

    magma_int_t num_threads = 1, timing = 1; // 1 = print timing
    magma_int_t L0nnz;

    #pragma omp parallel
    {
        num_threads = omp_get_max_threads();
    }
    CHECK(magma_smtransfer(A, &hA, A.memory_location, Magma_CPU, queue));

    // in case using fill-in
    if (precond->levels > 0) {
        CHECK(magma_ssymbilu(&hA, precond->levels, &hL, &LT , queue));
        magma_smfree(&LT, queue);
    }
    
    CHECK(magma_smatrix_tril(hA, &L, queue));
    CHECK(magma_smtransfer(L, &L0, A.memory_location, Magma_CPU, queue));
    CHECK(magma_smatrix_addrowindex(&L, queue)); 
    L0nnz=L.nnz;
    
    if (timing == 1) {
        printf("ilut_fill_ratio = %.6f;\n\n", precond->atol); 

        printf("performance_%d = [\n%%iter L.nnz U.nnz    ILU-Norm     candidat  resid     ILU-norm  selectad  add       transp1   sweep1    selectrm  remove    sweep2    transp2   total       accum\n", (int) num_threads);
    }

    //##########################################################################

    for (magma_int_t iters =0; iters<precond->sweeps; iters++) {
        t_rm=0.0; t_add=0.0; t_res=0.0; t_sweep1=0.0; t_sweep2=0.0; t_cand=0.0;
        t_transpose1=0.0; t_transpose2=0.0; t_selectrm=0.0;
        t_selectadd=0.0; t_nrm=0.0; t_total = 0.0;

        // step 1: find candidates
        start = magma_sync_wtime(queue);
        magma_smfree(&LT, queue);
        magma_scsrcoo_transpose(L, &LT, queue);
        end = magma_sync_wtime(queue); t_transpose1+=end-start;
        start = magma_sync_wtime(queue); 
        magma_sparict_candidates(L0, L, LT, &hL, queue);
        end = magma_sync_wtime(queue); t_cand=+end-start;

        // step 2: compute residuals (optional when adding all candidates)
        start = magma_sync_wtime(queue);
        magma_sparilut_residuals(hA, L, L, &hL, queue);
        end = magma_sync_wtime(queue); t_res=+end-start;
        start = magma_sync_wtime(queue);
        magma_smatrix_abssum(hL, &sumL, queue);
        sum = sumL*2;
        end = magma_sync_wtime(queue); t_nrm+=end-start;

        // step 3: add candidates
        start = magma_sync_wtime(queue);
        CHECK(magma_scsr_sort(&hL, queue));
        end = magma_sync_wtime(queue); t_selectadd+=end-start;
        start = magma_sync_wtime(queue);
        CHECK(magma_smatrix_cup( L, hL, &L_new, queue));  
        end = magma_sync_wtime(queue); t_add=+end-start;
        magma_smfree(&hL, queue);

        // step 4: sweep
        start = magma_sync_wtime(queue);
        CHECK(magma_sparict_sweep_sync(&hA, &L_new, queue));
        end = magma_sync_wtime(queue); t_sweep1+=end-start;

        // step 5: select threshold to remove elements
        start = magma_sync_wtime(queue);
        num_rmL = max((L_new.nnz-L0nnz*(1+(precond->atol-1.)
            *(iters+1)/precond->sweeps)), 0);
        // pre-select: ignore the diagonal entries
        CHECK(magma_sparilut_preselect(0, &L_new, &oneL, queue));
        if (num_rmL>0) {
            CHECK(magma_sparilut_set_thrs_randomselect(num_rmL, 
                &oneL, 0, &thrsL, queue));
        } else {
            thrsL = 0.0;
        }
        magma_smfree(&oneL, queue);
        end = magma_sync_wtime(queue); t_selectrm=end-start;
        
        // step 6: remove elements
        start = magma_sync_wtime(queue);
        CHECK(magma_sparilut_thrsrm(1, &L_new, &thrsL, queue));
        CHECK(magma_smatrix_swap(&L_new, &L, queue));
        magma_smfree(&L_new, queue);
        end = magma_sync_wtime(queue); t_rm=end-start;
        
        // step 7: sweep
        start = magma_sync_wtime(queue);
        CHECK(magma_sparict_sweep_sync(&hA, &L, queue));
        end = magma_sync_wtime(queue); t_sweep2+=end-start;

        if (timing == 1) {
            t_total = t_cand+t_res+t_nrm+t_selectadd+t_add+t_transpose1
            +t_sweep1+t_selectrm+t_rm+t_sweep2+t_transpose2;
            accum = accum + t_total;
            printf("%5lld %5lld %5lld  %.4e   %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e    %.2e\n",
                    (long long) iters, (long long) L.nnz, (long long) L.nnz, 
                    (float) sum, t_cand, t_res, t_nrm, t_selectadd, t_add, 
                    t_transpose1, t_sweep1, t_selectrm, t_rm, t_sweep2, 
                    t_transpose2, t_total, accum);
            fflush(stdout);
        }
    }

    if (timing == 1) {
        printf("]; \n");
        fflush(stdout);
    }
    //##########################################################################

    CHECK(magma_smtransfer(L, &precond->L, Magma_CPU, Magma_DEV , queue));
    CHECK(magma_s_cucsrtranspose(precond->L, &precond->U, queue));
    CHECK(magma_smtransfer(precond->L, &precond->M, Magma_DEV, Magma_DEV, 
        queue));
    
    if (precond->trisolver == 0 || precond->trisolver == Magma_CUSOLVE) {
        CHECK(magma_scumicgeneratesolverinfo(precond, queue));
    } else {
        //prepare for iterative solves

        // extract the diagonal of L into precond->d
        CHECK(magma_sjacobisetup_diagscal(precond->L, &precond->d, queue));
        CHECK(magma_svinit(&precond->work1, Magma_DEV, hA.num_rows, 1, 
            MAGMA_S_ZERO, queue));

        // extract the diagonal of U into precond->d2
        CHECK(magma_sjacobisetup_diagscal(precond->U, &precond->d2, queue));
        CHECK(magma_svinit(&precond->work2, Magma_DEV, hA.num_rows, 1, 
            MAGMA_S_ZERO, queue));
    }

cleanup:
    magma_smfree(&hA, queue);
    magma_smfree(&L0, queue);
    magma_smfree(&hL, queue);
    magma_smfree(&oneL, queue);
    magma_smfree(&L, queue);
    magma_smfree(&LT, queue);
    magma_smfree(&L_new, queue);
#endif
    return info;
}
