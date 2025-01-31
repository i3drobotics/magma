/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Hartwig Anzt

       @generated from sparse/src/zparict.cpp, normal z -> c, Sat Mar 27 20:33:05 2021
*/

#include "magmasparse_internal.h"
#ifdef _OPENMP
#include <omp.h>
#endif

#define PRECISION_c


/* For hipSPARSE, they use a separate complex type than for hipBLAS */
#ifdef HAVE_HIP
  #define hipblasComplex hipFloatComplex
#endif


// todo: make it spacific  
#if CUDA_VERSION >= 11000 || defined(HAVE_HIP)
#define cusparseCreateSolveAnalysisInfo(info) {;}
#else
#define cusparseCreateSolveAnalysisInfo(info)                                                   \
    CHECK_CUSPARSE( cusparseCreateSolveAnalysisInfo( info ))
#endif

// todo: info is passed; buf has to be passed 
#if CUDA_VERSION >= 11000 || defined(HAVE_HIP)
#define cusparseCcsrsv_analysis(handle, trans, m, nnz, descr, val, row, col, info)              \
    {                                                                                           \
        csrsv2Info_t linfo = 0;                                                                 \
        int bufsize;                                                                            \
        void *buf;                                                                              \
        cusparseCreateCsrsv2Info(&linfo);                                                       \
        cusparseCcsrsv2_bufferSize(handle, trans, m, nnz, descr, val, row, col,                 \
                                   linfo, &bufsize);                                            \
        if (bufsize > 0)                                                                        \
           magma_malloc(&buf, bufsize);                                                         \
        cusparseCcsrsv2_analysis(handle, trans, m, nnz, descr, val, row, col, linfo,            \
                                 CUSPARSE_SOLVE_POLICY_USE_LEVEL, buf);                         \
        if (bufsize > 0)                                                                        \
           magma_free(buf);                                                                     \
    }
#endif

// todo: check the info and linfo if we have to give it back; free memory?   
#if CUDA_VERSION >= 11000 || defined(HAVE_HIP)
#define cusparseCcsrsm_analysis(handle, op, rows, nnz, descrA, dval, drow, dcol, info )         \
    {                                                                                           \
        magmaFloatComplex alpha = MAGMA_C_ONE;                                                 \
        cuFloatComplex *B;                                                                     \
        csrsm2Info_t linfo;                                                                     \
        size_t bufsize;                                                                         \
        void *buf;                                                                              \
        cusparseCreateCsrsm2Info(&linfo);                                                       \
        cusparseCcsrsm2_bufferSizeExt(handle, 0, op, CUSPARSE_OPERATION_NON_TRANSPOSE,          \
                                      rows, 1, nnz, (const cuFloatComplex *)&alpha,            \
                                      descrA, dval, drow, dcol,                                 \
                                      B, rows, linfo, CUSPARSE_SOLVE_POLICY_NO_LEVEL, &bufsize);\
        if (bufsize > 0)                                                                        \
           magma_malloc(&buf, bufsize);                                                         \
        cusparseCcsrsm2_analysis(handle, 0, op, CUSPARSE_OPERATION_NON_TRANSPOSE,               \
                                 rows, 1, nnz, (const cuFloatComplex *)&alpha,                 \
                                 descrA, dval, drow, dcol,                                      \
                                 B, rows, linfo, CUSPARSE_SOLVE_POLICY_NO_LEVEL, buf);          \
        if (bufsize > 0)                                                                        \
           magma_free(buf);                                                                     \
    }
#endif


/***************************************************************************//**
    Purpose
    -------

    Prepares the iterative threshold Incomplete Cholesky preconditioner. 
    The strategy is interleaving a parallel fixed-point iteration that 
    approximates an incomplete factorization for a given nonzero pattern with a 
    procedure that adaptively changes the pattern. Much of this new algorithm 
    has fine-grained parallelism, and we show that it can efficiently exploit 
    the compute power of shared memory architectures.

    This is the routine used in the publication by Anzt, Chow, Dongarra:
    ''ParILUT - A new parallel threshold ILU factorization''
    submitted to SIAM SISC in 2016.

    This function requires OpenMP, and is only available if OpenMP is activated.

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
magma_cparict(
    magma_c_matrix A,
    magma_c_matrix b,
    magma_c_preconditioner *precond,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    
#ifdef _OPENMP

    real_Double_t start, end;
    real_Double_t t_rm=0.0, t_add=0.0, t_res=0.0, t_sweep1=0.0, t_sweep2=0.0, t_cand=0.0,
                    t_transpose1=0.0, t_transpose2=0.0, t_selectrm=0.0,
                    t_selectadd=0.0, t_nrm=0.0, t_total = 0.0, accum=0.0;
                    
    float sum, sumL;//, sumU, thrsL_old=1e9, thrsU_old=1e9;

    cusparseHandle_t cusparseHandle=NULL;
    cusparseMatDescr_t descrL=NULL;
    cusparseMatDescr_t descrU=NULL;
    magma_c_matrix hA={Magma_CSR}, A0={Magma_CSR}, hAT={Magma_CSR}, hL={Magma_CSR}, hU={Magma_CSR},
                    oneL={Magma_CSR}, LT={Magma_CSR},
                    L={Magma_CSR}, L_new={Magma_CSR};
    magma_c_matrix L0={Magma_CSR};  
    magma_int_t num_rmL;
    float thrsL = 0.0;

    magma_int_t num_threads, timing = 1; // print timing
    magma_int_t L0nnz;

    #pragma omp parallel
    {
        num_threads = omp_get_max_threads();
    }


    CHECK( magma_cmtransfer( A, &hA, A.memory_location, Magma_CPU, queue ));
    CHECK( magma_cmtransfer( A, &A0, A.memory_location, Magma_CPU, queue ));

        // in case using fill-in
    if( precond->levels > 0 ){
        CHECK( magma_csymbilu( &hA, precond->levels, &hL, &hU , queue ));
    }
    magma_cmfree(&hU, queue );
    L.diagorder_type = Magma_VALUE;
    magma_cmatrix_tril( hA, &L, queue );
    L.rowidx = NULL;
    magma_cmatrix_addrowindex( &L, queue ); 
    L0nnz=L.nnz;
        
    // need only lower triangular
    CHECK( magma_cmtransfer( L, &L0, A.memory_location, Magma_CPU, queue ));
    
    if (timing == 1) {
        printf("ilut_fill_ratio = %.6f;\n\n", precond->atol ); 

        printf("performance_%d = [\n%%iter L.nnz U.nnz    ILU-Norm     candidat  resid     ILU-norm  selectad  add       transp1   sweep1    selectrm  remove    sweep2    transp2   total       accum\n", (int) num_threads);
    }

    //##########################################################################

    for( magma_int_t iters =0; iters<precond->sweeps; iters++ ) {
    t_rm=0.0; t_add=0.0; t_res=0.0; t_sweep1=0.0; t_sweep2=0.0; t_cand=0.0;
                        t_transpose1=0.0; t_transpose2=0.0; t_selectrm=0.0;
                        t_selectadd=0.0; t_nrm=0.0; t_total = 0.0;
     
        num_rmL = max( (L_new.nnz-L0nnz*(1+precond->atol*(iters+1)/precond->sweeps)), 0 );
        start = magma_sync_wtime( queue );
        magma_cmfree(&LT, queue );
        magma_ccsrcoo_transpose( L, &LT, queue );
        end = magma_sync_wtime( queue ); t_transpose1+=end-start;
        start = magma_sync_wtime( queue ); 
        magma_cparict_candidates( L0, L, LT, &hL, queue );
        #pragma omp parallel        
        for(int row=0; row<hL.num_rows; row++){
            magma_cindexsort( &hL.col[hL.row[row]], 0, hL.row[row+1]-hL.row[row]-1, queue );
        }
        end = magma_sync_wtime( queue ); t_cand=+end-start;
        
        start = magma_sync_wtime( queue );
        magma_cparilut_residuals( hA, L, L, &hL, queue );
        end = magma_sync_wtime( queue ); t_res=+end-start;
        start = magma_sync_wtime( queue );
        magma_cmatrix_abssum( hL, &sumL, queue );
        sum = sumL*2;
        end = magma_sync_wtime( queue ); t_nrm+=end-start;
        
        start = magma_sync_wtime( queue );
        CHECK( magma_cmatrix_cup(  L, hL, &L_new, queue ) );  
        end = magma_sync_wtime( queue ); t_add=+end-start;
        magma_cmfree( &hL, queue );
       
        start = magma_sync_wtime( queue );
         CHECK( magma_cparict_sweep_sync( &A0, &L_new, queue ) );
        end = magma_sync_wtime( queue ); t_sweep1+=end-start;
        num_rmL = max( (L_new.nnz-L0nnz*(1+(precond->atol-1.)*(iters+1)/precond->sweeps)), 0 );
        start = magma_sync_wtime( queue );
        magma_cparilut_preselect( 0, &L_new, &oneL, queue );
        //#pragma omp parallel
        {
            if( num_rmL>0 ){
                magma_cparilut_set_thrs_randomselect( num_rmL, &oneL, 0, &thrsL, queue );
            } else {
                thrsL = 0.0;
            }
        }
        end = magma_sync_wtime( queue ); t_selectrm=end-start;
        magma_cmfree( &oneL, queue );
        start = magma_sync_wtime( queue );
        magma_cparilut_thrsrm( 1, &L_new, &thrsL, queue );//printf("done...");fflush(stdout);
        CHECK( magma_cmatrix_swap( &L_new, &L, queue) );
        magma_cmfree( &L_new, queue );
        end = magma_sync_wtime( queue ); t_rm=end-start;
        
        start = magma_sync_wtime( queue );
        CHECK( magma_cparict_sweep_sync( &A0, &L, queue ) );
        end = magma_sync_wtime( queue ); t_sweep2+=end-start;

        if( timing == 1 ){
            t_total = t_cand+t_res+t_nrm+t_selectadd+t_add+t_transpose1+t_sweep1+t_selectrm+t_rm+t_sweep2+t_transpose2;
            accum = accum + t_total;
            printf("%5lld %5lld %5lld  %.4e   %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e    %.2e\n",
                    (long long) iters, (long long) L.nnz, (long long) L.nnz, (float) sum, 
                    t_cand, t_res, t_nrm, t_selectadd, t_add, t_transpose1, t_sweep1, t_selectrm, t_rm, t_sweep2, t_transpose2, t_total, accum );
            fflush(stdout);
        }
    }

    if (timing == 1) {
        printf("]; \n");
    }
    //##########################################################################



    //printf("%% check L:\n"); fflush(stdout);
    //magma_cdiagcheck_cpu( hL, queue );
    //printf("%% check U:\n"); fflush(stdout);
    //magma_cdiagcheck_cpu( hU, queue );

    // for CUSPARSE
    CHECK( magma_cmtransfer( L, &precond->M, Magma_CPU, Magma_DEV , queue ));

    // CUSPARSE context //
    // lower triangular factor
    CHECK_CUSPARSE( cusparseCreate( &cusparseHandle ));
    CHECK_CUSPARSE( cusparseCreateMatDescr( &descrL ));
    CHECK_CUSPARSE( cusparseSetMatType( descrL, CUSPARSE_MATRIX_TYPE_TRIANGULAR ));
    CHECK_CUSPARSE( cusparseSetMatDiagType( descrL, CUSPARSE_DIAG_TYPE_NON_UNIT ));
    CHECK_CUSPARSE( cusparseSetMatIndexBase( descrL, CUSPARSE_INDEX_BASE_ZERO ));
    CHECK_CUSPARSE( cusparseSetMatFillMode( descrL, CUSPARSE_FILL_MODE_LOWER ));
    cusparseCreateSolveAnalysisInfo( &precond->cuinfoL );
    cusparseCcsrsv_analysis( cusparseHandle,
                             CUSPARSE_OPERATION_NON_TRANSPOSE, precond->M.num_rows,
                             precond->M.nnz, descrL,
                             (cuFloatComplex*)precond->M.dval, precond->M.drow, precond->M.dcol, 
                             precond->cuinfoL );
    
    // upper triangular factor
    CHECK_CUSPARSE( cusparseCreateMatDescr( &descrU ));
    CHECK_CUSPARSE( cusparseSetMatType( descrU, CUSPARSE_MATRIX_TYPE_TRIANGULAR ));
    CHECK_CUSPARSE( cusparseSetMatDiagType( descrU, CUSPARSE_DIAG_TYPE_NON_UNIT ));
    CHECK_CUSPARSE( cusparseSetMatIndexBase( descrU, CUSPARSE_INDEX_BASE_ZERO ));
    CHECK_CUSPARSE( cusparseSetMatFillMode( descrU, CUSPARSE_FILL_MODE_LOWER ));
    cusparseCreateSolveAnalysisInfo( &precond->cuinfoU );
    cusparseCcsrsm_analysis( cusparseHandle,
                             CUSPARSE_OPERATION_TRANSPOSE, precond->M.num_rows,
                             precond->M.nnz, descrU,
                             (cuFloatComplex*)precond->M.dval, precond->M.drow, precond->M.dcol, 
                             precond->cuinfoU );
    
    
    if( precond->trisolver != 0 && precond->trisolver != Magma_CUSOLVE ){
        //prepare for iterative solves
        
        // copy the matrix to precond->L and (transposed) to precond->U
        CHECK( magma_cmtransfer(precond->M, &(precond->L), Magma_DEV, Magma_DEV, queue ));
        CHECK( magma_cmtranspose( precond->L, &(precond->U), queue ));
        
        // extract the diagonal of L into precond->d
        CHECK( magma_cjacobisetup_diagscal( precond->L, &precond->d, queue ));
        CHECK( magma_cvinit( &precond->work1, Magma_DEV, hA.num_rows, 1, MAGMA_C_ZERO, queue ));
        
        // extract the diagonal of U into precond->d2
        CHECK( magma_cjacobisetup_diagscal( precond->U, &precond->d2, queue ));
        CHECK( magma_cvinit( &precond->work2, Magma_DEV, hA.num_rows, 1, MAGMA_C_ZERO, queue ));
    }

    if( precond->trisolver == Magma_JACOBI && precond->pattern == 1 ){
        // dirty workaround for Jacobi trisolves....
        magma_cmfree( &hL, queue );
        CHECK( magma_cmtransfer( precond->L, &hL, Magma_DEV, Magma_CPU , queue ));
        hAT.diagorder_type = Magma_VALUE;
        CHECK( magma_cmconvert( hL, &hAT , Magma_CSR, Magma_CSRU, queue ));
        #pragma omp parallel for
        for (magma_int_t i=0; i<hAT.nnz; i++) {
            hAT.val[i] = MAGMA_C_ONE/hAT.val[i];
        }
        CHECK( magma_cmtransfer( hAT, &(precond->LD), Magma_CPU, Magma_DEV, queue ));

    }

    cleanup:

    cusparseDestroy( cusparseHandle );
    cusparseDestroyMatDescr( descrL );
    cusparseDestroyMatDescr( descrU );
    cusparseHandle=NULL;
    descrL=NULL;
    descrU=NULL;
    magma_cmfree( &hA, queue );
    magma_cmfree( &hAT, queue );
    magma_cmfree( &A0, queue );
    magma_cmfree( &L0, queue );
    magma_cmfree( &hAT, queue );
    magma_cmfree( &hL, queue );
    magma_cmfree( &L, queue );
    magma_cmfree( &L_new, queue );
#endif
    return info;
}
