/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Hartwig Anzt

       @generated from sparse/src/zparilut.cpp, normal z -> c, Sat Mar 27 20:33:04 2021
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
        cusparseCcsrsv2_bufferSize(handle, trans, m, nnz, descr, (cuFloatComplex*)val, row, col,                 \
                                   linfo, &bufsize);                                            \
        if (bufsize > 0)                                                                        \
           magma_malloc(&buf, bufsize);                                                         \
        cusparseCcsrsv2_analysis(handle, trans, m, nnz, descr, (cuFloatComplex*)val, row, col, linfo,            \
                                 CUSPARSE_SOLVE_POLICY_USE_LEVEL, buf);                         \
        if (bufsize > 0)                                                                        \
           magma_free(buf);                                                                     \
    }
#endif

/***************************************************************************//**
    Purpose
    -------

    Prepares the iterative threshold Incomplete LU preconditioner. The strategy
    is interleaving a parallel fixed-point iteration that approximates an
    incomplete factorization for a given nonzero pattern with a procedure that
    adaptively changes the pattern. Much of this new algorithm has fine-grained
    parallelism, and we show that it can efficiently exploit the compute power
    of shared memory architectures.

    This is the routine used in the publication by Anzt, Chow, Dongarra:
    ''ParILUT - A new parallel threshold ILU factorization''
    submitted to SIAM SISC in 2017.

    This function requires OpenMP, and is only available if OpenMP is activated.
    
    The parameter list is:
    
    precond.sweeps : number of ParILUT steps
    precond.atol   : absolute fill ratio (1.0 keeps nnz constant)
    precond.rtol   : how many candidates are added to the sparsity pattern
                        * 1.0 one per row
                        * < 1.0 a fraction of those
                        * > 1.0 all candidates

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
magma_cparilut(
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
                    
    float sum, sumL, sumU;

    cusparseHandle_t cusparseHandle=NULL;
    cusparseMatDescr_t descrL=NULL;
    cusparseMatDescr_t descrU=NULL;
    magma_c_matrix hA={Magma_CSR}, A0={Magma_CSR}, hAT={Magma_CSR}, hL={Magma_CSR}, hU={Magma_CSR},
                    oneL={Magma_CSR}, oneU={Magma_CSR},
                    L={Magma_CSR}, U={Magma_CSR}, L_new={Magma_CSR}, U_new={Magma_CSR}, UT={Magma_CSR};
    magma_c_matrix L0={Magma_CSR}, U0={Magma_CSR};  
    magma_int_t num_rmL, num_rmU;
    float thrsL = 0.0;
    float thrsU = 0.0;

    magma_int_t num_threads, timing = 1; // print timing
    magma_int_t L0nnz, U0nnz;

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
    magma_cmfree(&hL, queue );
    L.diagorder_type = Magma_VALUE;
    magma_cmatrix_tril( hA, &L, queue );
    magma_cmtranspose(hA, &hAT, queue );
    U.diagorder_type = Magma_UNITY;
    magma_cmatrix_tril( hAT, &U, queue );
    for ( magma_int_t z=0; z<U.num_rows; z++ ){
        U.val[U.row[z+1]-1] = MAGMA_C_ONE;        
    }

    CHECK( magma_cmtranspose( U, &UT, queue) );
    L.rowidx = NULL;
    UT.rowidx = NULL;
    magma_cmatrix_addrowindex( &L, queue ); 
    magma_cmatrix_addrowindex( &U, queue ); 
    //CHECK( magma_cparilut_sweep( &A0, &L, &UT, queue ) );
    //CHECK( magma_cparilut_sweep( &A0, &L, &UT, queue ) );
    //CHECK( magma_cparilut_sweep( &A0, &L, &UT, queue ) );
    //CHECK( magma_cparilut_sweep( &A0, &L, &UT, queue ) );
    //CHECK( magma_cparilut_sweep( &A0, &L, &UT, queue ) );
    L0nnz=L.nnz;
    U0nnz=U.nnz;
        
    // need only lower triangular
    magma_cmfree(&U, queue );
    CHECK( magma_cmtranspose( UT, &U, queue) );
    CHECK( magma_cmtransfer( L, &L0, A.memory_location, Magma_CPU, queue ));
    CHECK( magma_cmtransfer( L, &oneL, A.memory_location, Magma_CPU, queue ));
    CHECK( magma_cmtransfer( UT, &U0, A.memory_location, Magma_CPU, queue ));
    magma_cmatrix_addrowindex( &U, queue );
    magma_cmfree(&UT, queue );
    //magma_free_cpu( UT.row ); UT.row = NULL;
    //magma_free_cpu( UT.list ); UT.list = NULL;
    //CHECK( magma_cparilut_create_collinkedlist( U, &UT, queue) );

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
        num_rmU = max( (U_new.nnz-U0nnz*(1+precond->atol*(iters+1)/precond->sweeps)), 0 );
        // magma_free_cpu( UT.row ); UT.row = NULL;
        // magma_free_cpu( UT.list ); UT.list = NULL;
        // CHECK( magma_cparilut_create_collinkedlist( U, &UT, queue) );
        start = magma_sync_wtime( queue );
        magma_cmfree(&UT, queue );
        //magma_cmtransposestruct_cpu( U, &UT, queue );
        magma_ccsrcoo_transpose( U, &UT, queue );
        end = magma_sync_wtime( queue ); t_transpose1+=end-start;
        start = magma_sync_wtime( queue );
        magma_cparilut_candidates( L0, U0, L, UT, &hL, &hU, queue );
        end = magma_sync_wtime( queue ); t_cand=+end-start;
        
        if( precond->rtol == 1.0 ){
            
            start = magma_sync_wtime( queue );
            magma_cparilut_residuals( hA, L, U, &hL, queue );
            magma_cparilut_residuals( hA, L, U, &hU, queue );
            end = magma_sync_wtime( queue ); t_res=+end-start;
            start = magma_sync_wtime( queue );
            magma_cmatrix_abssum( hL, &sumL, queue );
            magma_cmatrix_abssum( hU, &sumU, queue );
            sum = sumL + sumU;
            end = magma_sync_wtime( queue ); t_nrm+=end-start;
            
            
            start = magma_sync_wtime( queue );
            magma_cparilut_transpose_select_one( hU, &oneU, queue );
            magma_cparilut_selectoneperrow( 1, &hL, &oneL, queue );
            magma_cmfree(&hL, queue );
            magma_cmfree(&hU, queue );
            end = magma_sync_wtime( queue ); t_selectadd+=end-start;
            
        } else if( precond->rtol > 1.0 ) {
            start = magma_sync_wtime( queue );
            magma_cparilut_residuals( hA, L, U, &hL, queue );
            magma_cparilut_residuals( hA, L, U, &hU, queue );
            end = magma_sync_wtime( queue ); t_res=+end-start;
            start = magma_sync_wtime( queue );
            magma_cmatrix_abssum( hL, &sumL, queue );
            magma_cmatrix_abssum( hU, &sumU, queue );
            sum = sumL + sumU;
            end = magma_sync_wtime( queue ); t_nrm+=end-start;
            CHECK( magma_cmatrix_swap(  &hL, &oneL, queue) );
            magma_cmfree(&hL, queue );
            start = magma_sync_wtime( queue );
            if( precond->pattern == 2 ){
                // align residuals in U
                magma_cparilut_align_residuals( L, U, &hL, &hU, queue );
            } else if(precond->pattern == 0){
                for(magma_int_t z=0; z<hL.nnz; z++)
                    hL.val[z] = MAGMA_C_ZERO;
                for(magma_int_t z=0; z<hU.nnz; z++)
                    hU.val[z] = MAGMA_C_ZERO;
            }
            magma_ccsrcoo_transpose( hU, &oneU, queue );
            end = magma_sync_wtime( queue ); t_transpose2+=end-start;
            magma_cmfree(&hU, queue );
            magma_cmfree(&UT, queue );
                
        } else {
            
            start = magma_sync_wtime( queue );
            magma_cparilut_residuals( hA, L, U, &hL, queue );
            magma_cparilut_residuals( hA, L, U, &hU, queue );
            end = magma_sync_wtime( queue ); t_res=+end-start;
            start = magma_sync_wtime( queue );
            magma_cmatrix_abssum( hL, &sumL, queue );
            magma_cmatrix_abssum( hU, &sumU, queue );
            sum = sumL + sumU;
            end = magma_sync_wtime( queue ); t_nrm+=end-start;
            
            
            start = magma_sync_wtime( queue );
            magma_cparilut_transpose_select_one( hU, &oneU, queue );
            
            magma_cmfree(&hU, queue );
            magma_cmfree(&UT, queue );
            // magma_cmatrix_addrowindex( &hU, queue );
            // CHECK( magma_cmatrix_swap( &oneU, &hU, queue) );
            // magma_cmfree(&oneU, queue );
            // magma_cmatrix_addrowindex( &hU, queue );
            
            
            end = magma_sync_wtime( queue ); t_transpose2+=end-start;
            
        
            magma_cparilut_selectoneperrow( 1, &hL, &oneL, queue );
            //CHECK( magma_cmatrix_swap( &oneL, &hL, queue) );
            //CHECK( magma_cmatrix_swap( &oneU, &hU, queue) );  
        
            // use only a subset of the candidates
            //magma_cmfree( &oneL, queue );
            //magma_cmfree( &oneU, queue );
            num_rmL = max(oneL.nnz * ( precond->rtol ),0);
            num_rmU = max(oneU.nnz * ( precond->rtol ),0);
            // num_rmL = max(hL.nnz * ( precond->rtol-0.15*iters ),0);
            // num_rmU = max(hU.nnz * ( precond->rtol-0.15*iters ),0);
            //printf("hL:%d  hU:%d\n", num_rmL, num_rmU);
            //#pragma omp parallel
            {
              //  magma_int_t id = omp_get_thread_num();
                //if( id == 0 ){
                    if( num_rmL>0 ){
                        magma_cparilut_set_thrs_randomselect( num_rmL, &hL, 1, &thrsL, queue );
                    } else {
                        thrsL = 1e6;
                    }
                //} 
                //if( id == num_threads-1 ){
                    if( num_rmU>0 ){
                        magma_cparilut_set_thrs_randomselect( num_rmU, &hU, 1, &thrsU, queue );
                    } else {
                        thrsU = 1e6;
                    }
                //}
            }
            magma_cparilut_thrsrm( 1, &oneL, &thrsL, queue );
            magma_cparilut_thrsrm( 1, &oneU, &thrsU, queue );
            
        }
        
        end = magma_sync_wtime( queue ); t_selectadd+=end-start;
        
        start = magma_sync_wtime( queue );
        #pragma omp parallel        
        for(int row=0; row<hL.num_rows; row++){
            magma_cindexsort( &hL.col[hL.row[row]], 0, hL.row[row+1]-hL.row[row]-1, queue );
        }

        #pragma omp parallel  
        for(int row=0; row<hL.num_rows; row++){
            magma_cindexsort( &hU.col[hU.row[row]], 0, hU.row[row+1]-hU.row[row]-1, queue );
        }
        CHECK( magma_cmatrix_cup(  L, oneL, &L_new, queue ) );   
        CHECK( magma_cmatrix_cup(  U, oneU, &U_new, queue ) );
        //magma_cmatrix_addrowindex( &U, queue );
        end = magma_sync_wtime( queue ); t_add=+end-start;
        magma_cmfree( &oneL, queue );
        magma_cmfree( &oneU, queue );
       
        // using linked list
       // start = magma_sync_wtime( queue );
       // magma_free_cpu( UT.row ); UT.row = NULL;
       // magma_free_cpu( UT.list ); UT.list = NULL;
       // CHECK( magma_cparilut_create_collinkedlist( U_new, &UT, queue) );
       // end = magma_sync_wtime( queue ); t_transpose2+=end-start;
        start = magma_sync_wtime( queue );
        // CHECK( magma_cparilut_sweep( &A0, &L_new, &U_new, queue ) );
        
         CHECK( magma_cparilut_sweep_sync( &A0, &L_new, &U_new, queue ) );
        end = magma_sync_wtime( queue ); t_sweep1+=end-start;
        num_rmL = max( (L_new.nnz-L0nnz*(1+(precond->atol-1.)*(iters+1)/precond->sweeps)), 0 );
        num_rmU = max( (U_new.nnz-U0nnz*(1+(precond->atol-1.)*(iters+1)/precond->sweeps)), 0 );
        start = magma_sync_wtime( queue );
        // pre-select: ignore the diagonal entries
        magma_cparilut_preselect( 0, &L_new, &oneL, queue );
        magma_cparilut_preselect( 0, &U_new, &oneU, queue );
        //#pragma omp parallel
        {
          //  magma_int_t id = omp_get_thread_num();
            //if( id == 0 ){
                if( num_rmL>0 ){
                    magma_cparilut_set_thrs_randomselect( num_rmL, &oneL, 0, &thrsL, queue );
                } else {
                    thrsL = 0.0;
                }
            //} 
            //if( id == num_threads-1 ){
                if( num_rmU>0 ){
                    magma_cparilut_set_thrs_randomselect( num_rmU, &oneU, 0, &thrsU, queue );
                } else {
                    thrsU = 0.0;
                }
            //}
        }
        
        // if(thrsL > thrsL_old){
        //     thrsL=thrsL*2;
        // } else {
        //     thrsL_old=thrsL;
        // }
        // if(thrsU > thrsU_old){
        //     thrsU=thrsU*2;
        // } else {
        //     thrsU_old=thrsU;
        // }
        
        // magma_cparilut_set_thrs_randomselect( num_rmL, &L_new, 0, &thrsL, queue );
        // magma_cparilut_set_thrs_randomselect( num_rmU, &UT, 0, &thrsU, queue );
        end = magma_sync_wtime( queue ); t_selectrm=end-start;
        magma_cmfree( &oneL, queue );
        magma_cmfree( &oneU, queue );
        start = magma_sync_wtime( queue );
        
        magma_cparilut_thrsrm( 1, &L_new, &thrsL, queue );//printf("done...");fflush(stdout);
        magma_cparilut_thrsrm( 1, &U_new, &thrsU, queue );//printf("done...");fflush(stdout);

        
        // magma_cparilut_thrsrm_U( 1, L_new, &U_new, &thrsU, queue );
        // for(int z=0; z<L_new.nnz; z++){
        //     if(MAGMA_C_ABS(L_new.val[z])<thrsL){
        //      printf("invalid element here:%.4e  < %.4e  <%.4e> \n",MAGMA_C_ABS(L_new.val[z]),thrsL, MAGMA_C_REAL(L_new.val[z]));    
        //     }
        // }
        // 
        // for(int z=0; z<U_new.nnz; z++){
        //     if(MAGMA_C_ABS(U_new.val[z])<thrsU){
        //      printf("invalid element here:%.4e  < %.4e  <%.4e> \n",MAGMA_C_ABS(U_new.val[z]),thrsU, MAGMA_C_REAL(U_new.val[z]));    
        //     }
        // }
        
        // magma_cparilut_thrsrm_semilinked( &U_new, &UT, &thrsU, queue );//printf("done.\n");fflush(stdout);
        CHECK( magma_cmatrix_swap( &L_new, &L, queue) );
        CHECK( magma_cmatrix_swap( &U_new, &U, queue) );
        magma_cmfree( &L_new, queue );
        magma_cmfree( &U_new, queue );
        end = magma_sync_wtime( queue ); t_rm=end-start;
        
        start = magma_sync_wtime( queue );
        // magma_free_cpu( UT.row ); UT.row = NULL;
        // magma_free_cpu( UT.list ); UT.list = NULL;
        // CHECK( magma_cparilut_create_collinkedlist( U, &UT, queue) );
        // end = magma_sync_wtime( queue ); t_transpose1+=end-start;
        
        start = magma_sync_wtime( queue );
        
        //magma_free_cpu( U.rowidx ); U.rowidx = NULL;
        
        // CHECK( magma_cparilut_sweep( &A0, &L, &U, queue ) );
        CHECK( magma_cparilut_sweep_sync( &A0, &L, &U, queue ) );
        end = magma_sync_wtime( queue ); t_sweep2+=end-start;

        start = magma_sync_wtime( queue );

        end = magma_sync_wtime( queue ); t_rm+=end-start;
        // end using linked list
        
        if( timing == 1 ){
            t_total = t_cand+t_res+t_nrm+t_selectadd+t_add+t_transpose1+t_sweep1+t_selectrm+t_rm+t_sweep2+t_transpose2;
            accum = accum + t_total;
            printf("%5lld %5lld %5lld  %.4e   %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e  %.2e    %.2e\n",
                    (long long) iters, (long long) L.nnz, (long long) U.nnz, (float) sum, 
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
    CHECK( magma_cmtransfer( L, &precond->L, Magma_CPU, Magma_DEV , queue ));
    magma_ccsrcoo_transpose( U, &UT, queue );
    //magma_cmtranspose(U, &UT, queue );
    CHECK( magma_cmtransfer( UT, &precond->U, Magma_CPU, Magma_DEV , queue ));

    // CUSPARSE context //
    CHECK_CUSPARSE( cusparseCreate( &cusparseHandle ));
    CHECK_CUSPARSE( cusparseCreateMatDescr( &descrL ));
    CHECK_CUSPARSE( cusparseSetMatType( descrL, CUSPARSE_MATRIX_TYPE_TRIANGULAR ));
    CHECK_CUSPARSE( cusparseSetMatDiagType( descrL, CUSPARSE_DIAG_TYPE_NON_UNIT ));
    CHECK_CUSPARSE( cusparseSetMatIndexBase( descrL, CUSPARSE_INDEX_BASE_ZERO ));
    CHECK_CUSPARSE( cusparseSetMatFillMode( descrL, CUSPARSE_FILL_MODE_LOWER ));
    cusparseCreateSolveAnalysisInfo( &precond->cuinfoL );
    cusparseCcsrsv_analysis( cusparseHandle,
                             CUSPARSE_OPERATION_NON_TRANSPOSE, precond->L.num_rows,
                             precond->L.nnz, descrL,
                             precond->L.dval, precond->L.drow, precond->L.dcol, 
                             precond->cuinfoL );
    CHECK_CUSPARSE( cusparseCreateMatDescr( &descrU ));
    CHECK_CUSPARSE( cusparseSetMatType( descrU, CUSPARSE_MATRIX_TYPE_TRIANGULAR ));
    CHECK_CUSPARSE( cusparseSetMatDiagType( descrU, CUSPARSE_DIAG_TYPE_NON_UNIT ));
    CHECK_CUSPARSE( cusparseSetMatIndexBase( descrU, CUSPARSE_INDEX_BASE_ZERO ));
    CHECK_CUSPARSE( cusparseSetMatFillMode( descrU, CUSPARSE_FILL_MODE_UPPER ));
    cusparseCreateSolveAnalysisInfo( &precond->cuinfoU );
    cusparseCcsrsv_analysis( cusparseHandle,
                             CUSPARSE_OPERATION_NON_TRANSPOSE, precond->U.num_rows,
                             precond->U.nnz, descrU,
                             precond->U.dval, precond->U.drow, precond->U.dcol, 
                             precond->cuinfoU );

    if( precond->trisolver != 0 && precond->trisolver != Magma_CUSOLVE ){
        //prepare for iterative solves

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
        magma_cmfree( &hU, queue );
        CHECK( magma_cmtransfer( precond->U, &hU, Magma_DEV, Magma_CPU , queue ));
        CHECK( magma_cmtransfer( precond->L, &hL, Magma_DEV, Magma_CPU , queue ));
        magma_cmfree( &hAT, queue );
        hAT.diagorder_type = Magma_VALUE;
        CHECK( magma_cmconvert( hL, &hAT , Magma_CSR, Magma_CSRU, queue ));
        #pragma omp parallel for
        for (magma_int_t i=0; i<hAT.nnz; i++) {
            hAT.val[i] = MAGMA_C_ONE/hAT.val[i];
        }
        CHECK( magma_cmtransfer( hAT, &(precond->LD), Magma_CPU, Magma_DEV, queue ));

        magma_cmfree( &hAT, queue );
        hAT.diagorder_type = Magma_VALUE;
        CHECK( magma_cmconvert( hU, &hAT , Magma_CSR, Magma_CSRL, queue ));
        #pragma omp parallel for
        for (magma_int_t i=0; i<hAT.nnz; i++) {
            hAT.val[i] = MAGMA_C_ONE/hAT.val[i];
        }
        CHECK( magma_cmtransfer( hAT, &(precond->UD), Magma_CPU, Magma_DEV, queue ));
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
    magma_cmfree( &U0, queue );
    magma_cmfree( &hAT, queue );
    magma_cmfree( &hL, queue );
    magma_cmfree( &L, queue );
    magma_cmfree( &L_new, queue );
    magma_cmfree( &hU, queue );
    magma_cmfree( &U, queue );
    magma_cmfree( &UT, queue );
    magma_cmfree( &U_new, queue );
    //magma_cmfree( &UT, queue );
#endif
    return info;
}
