/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Hartwig Anzt

       @generated from sparse/src/zpcgs.cpp, normal z -> d, Sat Mar 27 20:32:58 2021
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
    This is a GPU implementation of the preconditioned Conjugate
    Gradient Squared (CGS) method.

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
                preconditioner
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_dgesv
    ********************************************************************/

extern "C" magma_int_t
magma_dpcgs(
    magma_d_matrix A, magma_d_matrix b, magma_d_matrix *x,
    magma_d_solver_par *solver_par,
    magma_d_preconditioner *precond_par,
    magma_queue_t queue )
{
    magma_int_t info = MAGMA_NOTCONVERGED;
    
    // prepare solver feedback
    solver_par->solver = Magma_PCGS;
    solver_par->numiter = 0;
    solver_par->spmv_count = 0;
    
    // constants
    const double c_zero    = MAGMA_D_ZERO;
    const double c_one     = MAGMA_D_ONE;
    const double c_neg_one = MAGMA_D_NEG_ONE;
    
    // solver variables
    double nom0, r0, res=0, nomb;
    double rho, rho_l = c_one, alpha, beta;
    
    magma_int_t dofs = A.num_rows* b.num_cols;

    // GPU workspace
    magma_d_matrix r={Magma_CSR}, rt={Magma_CSR}, r_tld={Magma_CSR},
                    p={Magma_CSR}, q={Magma_CSR}, u={Magma_CSR}, v={Magma_CSR},  t={Magma_CSR},
                    p_hat={Magma_CSR}, q_hat={Magma_CSR}, u_hat={Magma_CSR}, v_hat={Magma_CSR};
    CHECK( magma_dvinit( &r, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &rt,Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &r_tld,Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &p, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &p_hat, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &q, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &q_hat, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &u, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &u_hat, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &v, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &v_hat, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &t, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));

    // solver setup
    CHECK(  magma_dresidualvec( A, b, *x, &r, &nom0, queue));
    magma_dcopy( dofs, r.dval, 1, r_tld.dval, 1, queue );   

    solver_par->init_res = nom0;
            
    nomb = magma_dnrm2( dofs, b.dval, 1, queue );
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

    //Chronometry
    real_Double_t tempo1, tempo2;
    tempo1 = magma_sync_wtime( queue );
    
    solver_par->numiter = 0;
    solver_par->spmv_count = 0;
    // start iteration
    do
    {
        solver_par->numiter++;
        
        rho = magma_ddot( dofs, r_tld.dval, 1, r.dval, 1, queue );
                                                            // rho = < r,r_tld>    
        if( magma_d_isnan_inf( rho ) ){
            info = MAGMA_DIVERGENCE;
            break;
        }
        
        if ( solver_par->numiter > 1 ) {                        // direction vectors
            beta = rho / rho_l;            
            magma_dcopy( dofs, r.dval, 1, u.dval, 1, queue );          // u = r
            magma_daxpy( dofs,  beta, q.dval, 1, u.dval, 1, queue );     // u = r + beta q
            magma_dscal( dofs, beta, p.dval, 1, queue );                 // p = beta*p
            magma_daxpy( dofs, c_one, q.dval, 1, p.dval, 1, queue );      // p = q + beta*p
            magma_dscal( dofs, beta, p.dval, 1, queue );                 // p = beta*(q + beta*p)
            magma_daxpy( dofs, c_one, u.dval, 1, p.dval, 1, queue );     // p = u + beta*(q + beta*p)
        //u = r + beta*q;
        //p = u + beta*( q + beta*p );
        }
        else{
            magma_dcopy( dofs, r.dval, 1, u.dval, 1, queue );          // u = r
            magma_dcopy( dofs, r.dval, 1, p.dval, 1, queue );          // p = r
        }
        // preconditioner
        CHECK( magma_d_applyprecond_left( MagmaNoTrans, A, p, &rt, precond_par, queue ));
        CHECK( magma_d_applyprecond_right( MagmaNoTrans, A, rt, &p_hat, precond_par, queue ));
        // SpMV
        CHECK( magma_d_spmv( c_one, A, p_hat, c_zero, v_hat, queue ));   // v = A p
        solver_par->spmv_count++;
        alpha = rho / magma_ddot( dofs, r_tld.dval, 1, v_hat.dval, 1, queue );
        magma_dcopy( dofs, u.dval, 1, q.dval, 1, queue );              // q = u
        magma_daxpy( dofs,  -alpha, v_hat.dval, 1, q.dval, 1, queue );   // q = u - alpha v_hat
        
        magma_dcopy( dofs, u.dval, 1, t.dval, 1, queue );             // t = q
        magma_daxpy( dofs,  c_one, q.dval, 1, t.dval, 1, queue );       // t = u + q
        // preconditioner
        CHECK( magma_d_applyprecond_left( MagmaNoTrans, A, t, &rt, precond_par, queue ));
        CHECK( magma_d_applyprecond_right( MagmaNoTrans, A, rt, &u_hat, precond_par, queue ));
        // SpMV
        CHECK( magma_d_spmv( c_one, A, u_hat, c_zero, t, queue ));   // t = A u_hat
        solver_par->spmv_count++;
        magma_daxpy( dofs,  alpha, u_hat.dval, 1, x->dval, 1, queue );     // x = x + alpha u_hat
        magma_daxpy( dofs,  c_neg_one*alpha, t.dval, 1, r.dval, 1, queue );       // r = r -alpha*A u_hat
        
        res = magma_dnrm2( dofs, r.dval, 1, queue );
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
        rho_l = rho;
    }
    while ( solver_par->numiter+1 <= solver_par->maxiter );
    
    tempo2 = magma_sync_wtime( queue );
    solver_par->runtime = (real_Double_t) tempo2-tempo1;
    double residual;
    CHECK(  magma_dresidualvec( A, b, *x, &r, &residual, queue));
    solver_par->iter_res = res;
    solver_par->final_res = residual;

    if ( solver_par->numiter < solver_par->maxiter && info == MAGMA_SUCCESS ) {
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
    magma_dmfree(&r, queue );
    magma_dmfree(&rt, queue );
    magma_dmfree(&r_tld, queue );
    magma_dmfree(&p, queue );
    magma_dmfree(&q, queue );
    magma_dmfree(&u, queue );
    magma_dmfree(&v, queue );
    magma_dmfree(&t, queue );
    magma_dmfree(&p_hat, queue );
    magma_dmfree(&q_hat, queue );
    magma_dmfree(&u_hat, queue );
    magma_dmfree(&v_hat, queue );

    solver_par->info = info;
    return info;
}   /* magma_dpcgs */
