/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Hartwig Anzt

       @generated from sparse/src/zcg_merge.cpp, normal z -> d, Sat Mar 27 20:32:50 2021
*/

#include "magmasparse_internal.h"

#define RTOLERANCE     lapackf77_dlamch( "E" )
#define ATOLERANCE     lapackf77_dlamch( "E" )


/**
    Purpose
    -------

    Solves a system of linear equations
       A * X = B
    where A is a real matrix A.
    This is a GPU implementation of the Conjugate Gradient method in variant,
    where multiple operations are merged into one compute kernel.

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
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_dposv
    ********************************************************************/

extern "C" magma_int_t
magma_dcg_merge(
    magma_d_matrix A, magma_d_matrix b, magma_d_matrix *x,
    magma_d_solver_par *solver_par,
    magma_queue_t queue )
{
    magma_int_t info = MAGMA_NOTCONVERGED;
    
    // prepare solver feedback
    solver_par->solver = Magma_CGMERGE;
    solver_par->numiter = 0;
    solver_par->spmv_count = 0;
    
    // solver variables
    double alpha, beta, gamma, rho, tmp1, *skp_h={0};
    double nom, nom0, betanom, den, nomb;

    // some useful variables
    double c_zero = MAGMA_D_ZERO, c_one = MAGMA_D_ONE;
    magma_int_t dofs = A.num_rows*b.num_cols;

    magma_d_matrix r={Magma_CSR}, d={Magma_CSR}, z={Magma_CSR}, B={Magma_CSR}, C={Magma_CSR};
    double *d1=NULL, *d2=NULL, *skp=NULL;

    // GPU workspace
    CHECK( magma_dvinit( &r, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &d, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &z, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    
    CHECK( magma_dmalloc( &d1, dofs*(1) ));
    CHECK( magma_dmalloc( &d2, dofs*(1) ));
    // array for the parameters
    CHECK( magma_dmalloc( &skp, 6 ));
    // skp = [alpha|beta|gamma|rho|tmp1|tmp2]
    
    // solver setup
    magma_dscal( dofs, c_zero, x->dval, 1, queue );                      // x = 0
    //CHECK(  magma_dresidualvec( A, b, *x, &r, nom0, queue));
    magma_dcopy( dofs, b.dval, 1, r.dval, 1, queue );                    // r = b
    magma_dcopy( dofs, r.dval, 1, d.dval, 1, queue );                    // d = r
    nom0 = betanom = magma_dnrm2( dofs, r.dval, 1, queue );
    nom = nom0 * nom0;                                           // nom = r' * r
    CHECK( magma_d_spmv( c_one, A, d, c_zero, z, queue ));              // z = A d
    den = MAGMA_D_ABS( magma_ddot( dofs, d.dval, 1, z.dval, 1, queue ) ); // den = d'* z
    solver_par->init_res = nom0;
    
    nomb = magma_dnrm2( dofs, b.dval, 1, queue );
    if ( nomb == 0.0 ){
        nomb=1.0;
    }       
    
    // array on host for the parameters
    CHECK( magma_dmalloc_cpu( &skp_h, 6 ));
    
    alpha = rho = gamma = tmp1 = c_one;
    beta =  magma_ddot( dofs, r.dval, 1, r.dval, 1, queue );
    skp_h[0]=alpha;
    skp_h[1]=beta;
    skp_h[2]=gamma;
    skp_h[3]=rho;
    skp_h[4]=tmp1;
    skp_h[5]=MAGMA_D_MAKE(nom, 0.0);

    magma_dsetvector( 6, skp_h, 1, skp, 1, queue );

    if( nom0 < solver_par->atol ||
        nom0/nomb < solver_par->rtol ){
        info = MAGMA_SUCCESS;
        goto cleanup;
    }
    solver_par->final_res = solver_par->init_res;
    solver_par->iter_res = solver_par->init_res;
    if ( solver_par->verbose > 0 ) {
        solver_par->res_vec[0] = (real_Double_t) nom0;
        solver_par->timing[0] = 0.0;
    }
    // check positive definite
    if (den <= 0.0) {
        info = MAGMA_NONSPD; 
        goto cleanup;
    }
    
    //Chronometry
    real_Double_t tempo1, tempo2;
    tempo1 = magma_sync_wtime( queue );

    solver_par->numiter = 0;
    solver_par->spmv_count = 0;
    // start iteration
    do
    {
        solver_par->numiter++;

        // computes SpMV and dot product
        CHECK( magma_dcgmerge_spmv1(  A, d1, d2, d.dval, z.dval, skp, queue ));
        solver_par->spmv_count++;
        // updates x, r, computes scalars and updates d
        CHECK( magma_dcgmerge_xrbeta( dofs, d1, d2, x->dval, r.dval, d.dval, z.dval, skp, queue ));

        // check stopping criterion (asynchronous copy)
        magma_dgetvector( 1 , skp+1, 1, skp_h+1, 1, queue );
        betanom = sqrt(MAGMA_D_ABS(skp_h[1]));

        if ( solver_par->verbose > 0 ) {
            tempo2 = magma_sync_wtime( queue );
            if ( (solver_par->numiter)%solver_par->verbose==0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) betanom;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) tempo2-tempo1;
            }
        }

        if (  betanom  < solver_par->atol || 
              betanom/nomb < solver_par->rtol ) {
            break;
        }
    }
    while ( solver_par->numiter+1 <= solver_par->maxiter );
    
    tempo2 = magma_sync_wtime( queue );
    solver_par->runtime = (real_Double_t) tempo2-tempo1;
    double residual;
    CHECK(  magma_dresidualvec( A, b, *x, &r, &residual, queue));
    solver_par->iter_res = betanom;
    solver_par->final_res = residual;

    if ( solver_par->numiter < solver_par->maxiter ) {
        info = MAGMA_SUCCESS;
    } else if ( solver_par->init_res > solver_par->final_res ) {
        if ( solver_par->verbose > 0 ) {
            if ( (solver_par->numiter)%solver_par->verbose==0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) betanom;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) tempo2-tempo1;
            }
        }
        info = MAGMA_SLOW_CONVERGENCE;
        if( solver_par->iter_res < solver_par->atol ||
            solver_par->iter_res/solver_par->init_res < solver_par->rtol ){
            info = MAGMA_SUCCESS;
        }
    }
    else {
        if ( solver_par->verbose > 0 ) {
            if ( (solver_par->numiter)%solver_par->verbose==0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) betanom;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) tempo2-tempo1;
            }
        }
        solver_par->info = MAGMA_DIVERGENCE;
    }
    
cleanup:
    magma_dmfree(&r, queue );
    magma_dmfree(&z, queue );
    magma_dmfree(&d, queue );
    magma_dmfree(&B, queue );
    magma_dmfree(&C, queue );

    magma_free( d1 );
    magma_free( d2 );
    magma_free( skp );
    magma_free_cpu( skp_h );

    solver_par->info = info;
    return info;
}   /* magma_dcg_merge */
