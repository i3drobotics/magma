/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Hartwig Anzt

       @generated from sparse/src/zpcg.cpp, normal z -> s, Sat Mar 27 20:32:57 2021
*/

#include "magmasparse_internal.h"

#define RTOLERANCE     lapackf77_slamch( "E" )
#define ATOLERANCE     lapackf77_slamch( "E" )


/*******************************************************************************
    Purpose
    -------

    Solves a system of linear equations
       A * X = B
    where A is a real symmetric N-by-N positive definite matrix A.
    This is a GPU implementation of the preconditioned Conjugate
    Gradient method.

    Arguments
    ---------

    @param[in]
    A           magma_s_matrix
                input matrix A

    @param[in]
    b           magma_s_matrix
                RHS b

    @param[in,out]
    x           magma_s_matrix*
                solution approximation

    @param[in,out]
    solver_par  magma_s_solver_par*
                solver parameters

    @param[in]
    precond_par magma_s_preconditioner*
                preconditioner
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_sposv
*******************************************************************************/

extern "C" magma_int_t
magma_spcg(
    magma_s_matrix A, magma_s_matrix b, magma_s_matrix *x,
    magma_s_solver_par *solver_par,
    magma_s_preconditioner *precond_par,
    magma_queue_t queue )
{
    magma_int_t info = MAGMA_NOTCONVERGED;
    
    // prepare solver feedback
    solver_par->solver = Magma_PCG;
    solver_par->numiter = 0;
    solver_par->spmv_count = 0;
    
    // solver variables
    float alpha, beta;
    float nom0, r0,  res, nomb;
    float den, gammanew, gammaold = MAGMA_S_MAKE(1.0,0.0);
    // local variables
    float c_zero = MAGMA_S_ZERO, c_one = MAGMA_S_ONE;
    
    magma_int_t dofs = A.num_rows* b.num_cols;

    // GPU workspace
    magma_s_matrix r={Magma_CSR}, rt={Magma_CSR}, p={Magma_CSR}, q={Magma_CSR}, h={Magma_CSR};
    CHECK( magma_svinit( &r, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &rt,Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &p, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &q, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_svinit( &h, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    

    // solver setup
    CHECK(  magma_sresidualvec( A, b, *x, &r, &nom0, queue));

    // preconditioner
    CHECK( magma_s_applyprecond_left( MagmaNoTrans, A, r, &rt, precond_par, queue ));
    CHECK( magma_s_applyprecond_right( MagmaNoTrans, A, rt, &h, precond_par, queue ));

    magma_scopy( dofs, h.dval, 1, p.dval, 1, queue );                    // p = h
    CHECK( magma_s_spmv( c_one, A, p, c_zero, q, queue ));             // q = A p
    solver_par->spmv_count++;
    den =  magma_sdot( dofs, p.dval, 1, q.dval, 1, queue ); // den = p dot q
    solver_par->init_res = nom0;
            
    nomb = magma_snrm2( dofs, b.dval, 1, queue );
    if ( nomb == 0.0 ){
        nomb=1.0;
    }       
    if ( (r0 = nomb * solver_par->rtol) < ATOLERANCE ){
        r0 = ATOLERANCE;
    }
    solver_par->final_res = solver_par->init_res;
    solver_par->iter_res = solver_par->init_res;
    if ( solver_par->verbose > 0 ) {
        solver_par->res_vec[0] = (real_Double_t)nom0;
        solver_par->timing[0] = 0.0;
    }
    if ( nom0 < r0 ) {
        info = MAGMA_SUCCESS;
        goto cleanup;
    }
    // check positive definite
    if ( MAGMA_S_ABS(den) <= 0.0 ) {
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

        // preconditioner
        CHECK( magma_s_applyprecond_left( MagmaNoTrans, A, r, &rt, precond_par, queue ));
        CHECK( magma_s_applyprecond_right( MagmaNoTrans, A, rt, &h, precond_par, queue ));
        
        gammanew = magma_sdot( dofs, r.dval, 1, h.dval, 1, queue );
                                                            // gn = < r,h>

        if ( solver_par->numiter == 1 ) {
            magma_scopy( dofs, h.dval, 1, p.dval, 1, queue );                    // p = h
        } else {
            beta = (gammanew/gammaold);       // beta = gn/go
            magma_sscal( dofs, beta, p.dval, 1, queue );            // p = beta*p
            magma_saxpy( dofs, c_one, h.dval, 1, p.dval, 1, queue ); // p = p + h
        }

        CHECK( magma_s_spmv( c_one, A, p, c_zero, q, queue ));   // q = A p
        solver_par->spmv_count++;
        den = magma_sdot( dofs, p.dval, 1, q.dval, 1, queue );
                // den = p dot q

        alpha = gammanew / den;
        magma_saxpy( dofs,  alpha, p.dval, 1, x->dval, 1, queue );     // x = x + alpha p
        magma_saxpy( dofs, -alpha, q.dval, 1, r.dval, 1, queue );      // r = r - alpha q
        gammaold = gammanew;

        res = magma_snrm2( dofs, r.dval, 1, queue );
        if ( solver_par->verbose > 0 ) {
            tempo2 = magma_sync_wtime( queue );
            if ( (solver_par->numiter)%solver_par->verbose == 0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) res;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) tempo2-tempo1;
            }
        }

        if ( res/nomb <= solver_par->rtol || res <= solver_par->atol ){
            break;
        }
    }
    while ( solver_par->numiter+1 <= solver_par->maxiter );
    
    tempo2 = magma_sync_wtime( queue );
    solver_par->runtime = (real_Double_t) tempo2-tempo1;
    float residual;
    CHECK(  magma_sresidualvec( A, b, *x, &r, &residual, queue));
    solver_par->iter_res = res;
    solver_par->final_res = residual;

    if ( solver_par->numiter < solver_par->maxiter ) {
        info = MAGMA_SUCCESS;
    } else if ( solver_par->init_res > solver_par->final_res ) {
        if ( solver_par->verbose > 0 ) {
            if ( (solver_par->numiter)%solver_par->verbose == 0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) res;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) tempo2-tempo1;
            }
        }
        info = MAGMA_SLOW_CONVERGENCE;
        if( solver_par->iter_res < solver_par->rtol*nomb ||
            solver_par->iter_res < solver_par->atol ) {
            info = MAGMA_SUCCESS;
        }
    }
    else {
        if ( solver_par->verbose > 0 ) {
            if ( (solver_par->numiter)%solver_par->verbose == 0 ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) res;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) tempo2-tempo1;
            }
        }
        info = MAGMA_DIVERGENCE;
    }
    
cleanup:
    magma_smfree(&r, queue );
    magma_smfree(&rt, queue );
    magma_smfree(&p, queue );
    magma_smfree(&q, queue );
    magma_smfree(&h, queue );

    solver_par->info = info;
    return info;
}   /* magma_scg */
