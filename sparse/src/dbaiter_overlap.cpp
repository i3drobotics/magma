/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Hartwig Anzt

       @generated from sparse/src/zbaiter_overlap.cpp, normal z -> d, Sat Mar 27 20:32:56 2021
*/

#include "magmasparse_internal.h"

#define RTOLERANCE     lapackf77_dlamch( "E" )
#define ATOLERANCE     lapackf77_dlamch( "E" )


/**
    Purpose
    -------

    Solves a system of linear equations
       A * x = b
    via the block asynchronous iteration method on GPU.
    It used restricted additive Schwarz overlap in top-down direction.

    Arguments
    ---------

    @param[in]
    A           magma_d_matrix
                input matrix A

    @param[in]
    b           magma_d_matrix
                RHS b

    @param[in,out]
    x           magma_d_matrix*
                solution approximation

    @param[in,out]
    solver_par  magma_d_solver_par*
                solver parameters
                
    @param[in]
    precond_par magma_d_preconditioner*
                preconditioner parameters

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_dgesv
    ********************************************************************/

extern "C" magma_int_t
magma_dbaiter_overlap(
    magma_d_matrix A,
    magma_d_matrix b,
    magma_d_matrix *x,
    magma_d_solver_par *solver_par,
    magma_d_preconditioner *precond_par,
    magma_queue_t queue )
{
    magma_int_t info = MAGMA_NOTCONVERGED;
        
    // prepare solver feedback
    solver_par->solver = Magma_BAITERO;
    
    // some useful variables 
    double c_zero = MAGMA_D_ZERO;

    // initial residual
    real_Double_t tempo1, tempo2, runtime=0;
    double residual;
    magma_int_t localiter = precond_par->maxiter;
    
    magma_d_matrix Ah={Magma_CSR}, ACSR={Magma_CSR}, dA={Magma_CSR}, r={Magma_CSR},
        D={Magma_CSR}, R={Magma_CSR};
        

        
    // setup
    magma_int_t matrices;
        matrices = precond_par->levels;
    struct magma_d_matrix D_d[ 256 ];
    struct magma_d_matrix R_d[ 256 ];
    magma_int_t overlap;
    magma_int_t blocksize = 256;
    if(  matrices==2 ||
         matrices==4 ||
         matrices==8 ||
         matrices==16 ||
         matrices==32 ||
         matrices==64 ||
         matrices==128 ){
        overlap = blocksize/matrices;
    }else if( matrices == 1){
        overlap = 0;
    }else{
        printf("error: overlap ratio not supported.\n");
        goto cleanup;
    }

    CHECK( magma_dmtransfer( A, &Ah, A.memory_location, Magma_CPU, queue ));
    CHECK( magma_dmconvert( Ah, &ACSR, Ah.storage_type, Magma_CSR, queue ));

    CHECK( magma_dmtransfer( ACSR, &dA, Magma_CPU, Magma_DEV, queue ));
    
    CHECK( magma_dvinit( &r, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK(  magma_dresidualvec( dA, b, *x, &r, &residual, queue));
    solver_par->init_res = residual;
    if ( solver_par->verbose > 0 ) {
        solver_par->res_vec[0] = (real_Double_t) residual;
    }
    
    // setup  
    for( int i=0; i<matrices; i++ ){
        CHECK( magma_dcsrsplit( i*overlap, 256, ACSR, &D, &R, queue ));
        CHECK( magma_dmtransfer( D, &D_d[i], Magma_CPU, Magma_DEV, queue ));
        CHECK( magma_dmtransfer( R, &R_d[i], Magma_CPU, Magma_DEV, queue ));
        magma_dmfree(&D, queue );
        magma_dmfree(&R, queue );
    }
    

    
    magma_int_t iterinc;
    if( solver_par->verbose == 0 ){
        iterinc = solver_par->maxiter;
    }
    else{
        iterinc = solver_par->verbose;
    }
    solver_par->numiter = 0;
    solver_par->spmv_count = 0;
    // block-asynchronous iteration iterator
    do
    {
        tempo1 = magma_sync_wtime( queue );
        solver_par->numiter+= iterinc;
        for( int z=0; z<iterinc; z++){
            CHECK( magma_dbajac_csr_overlap( localiter, matrices, overlap, D_d, R_d, b, x, queue ));
        }
        tempo2 = magma_sync_wtime( queue );
        runtime += tempo2-tempo1;
        if ( solver_par->verbose > 0 ) {
        CHECK(  magma_dresidualvec( dA, b, *x, &r, &residual, queue));
            solver_par->res_vec[(solver_par->numiter)/solver_par->verbose]
                = (real_Double_t) residual;
            solver_par->timing[(solver_par->numiter)/solver_par->verbose]
                = (real_Double_t) runtime;
        }
    }
    while ( solver_par->numiter+1 <= solver_par->maxiter );

    solver_par->runtime = runtime;
    CHECK(  magma_dresidual( dA, b, *x, &residual, queue));
    solver_par->final_res = residual;
    solver_par->numiter = solver_par->maxiter;

    if ( solver_par->init_res > solver_par->final_res ){
        info = MAGMA_SUCCESS;
    }
    else {
        info = MAGMA_DIVERGENCE;
    }
    
cleanup:
    magma_dmfree(&r, queue );
    magma_dmfree(&D, queue );
    magma_dmfree(&R, queue );
    for( int i=0; i<matrices; i++ ){
        magma_dmfree(&D_d[i], queue );
        magma_dmfree(&R_d[i], queue );
    }
    magma_dmfree(&dA, queue );
    magma_dmfree(&ACSR, queue );
    magma_dmfree(&Ah, queue );

    solver_par->info = info;
    return info;
}   /* magma_dbaiter_overlap */
