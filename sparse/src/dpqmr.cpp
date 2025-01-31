/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Hartwig Anzt

       @generated from sparse/src/zpqmr.cpp, normal z -> d, Sat Mar 27 20:32:53 2021
*/

#include "magmasparse_internal.h"

#define RTOLERANCE     lapackf77_dlamch( "E" )
#define ATOLERANCE     lapackf77_dlamch( "E" )


/**
    Purpose
    -------

    Solves a system of linear equations
       A * X = B
    where A is a general real matrix A.
    This is a GPU implementation of the preconditioned 
    Quasi-Minimal Residual method (QMR).

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
magma_dpqmr(
    magma_d_matrix A, magma_d_matrix b, magma_d_matrix *x,
    magma_d_solver_par *solver_par,
    magma_d_preconditioner *precond_par,
    magma_queue_t queue )
{
    magma_int_t info = MAGMA_NOTCONVERGED;
    
    // prepare solver feedback
    solver_par->solver = Magma_QMR;
    solver_par->numiter = 0;
    solver_par->spmv_count = 0;
    
    
    // local variables
    double c_zero = MAGMA_D_ZERO, c_one = MAGMA_D_ONE;
    // solver variables
    double nom0, r0, res=0.0, nomb;
    double rho = c_one, rho1 = c_one, eta = -c_one , pds = c_one, 
                        thet = c_one, thet1 = c_one, epsilon = c_one, 
                        beta = c_one, delta = c_one, pde = c_one, rde = c_one,
                        gamm = c_one, gamm1 = c_one, psi = c_one;
    
    magma_int_t dofs = A.num_rows* b.num_cols;

    // need to transpose the matrix
    magma_d_matrix AT={Magma_CSR}, Ah1={Magma_CSR}, Ah2={Magma_CSR};
    
    // GPU workspace
    magma_d_matrix r={Magma_CSR}, r_tld={Magma_CSR},
                    v={Magma_CSR}, w={Magma_CSR}, wt={Magma_CSR},
                    d={Magma_CSR}, s={Magma_CSR}, z={Magma_CSR}, q={Magma_CSR}, 
                    p={Magma_CSR}, pt={Magma_CSR}, y={Magma_CSR},
                    vt={Magma_CSR}, yt={Magma_CSR}, zt={Magma_CSR};
    CHECK( magma_dvinit( &r, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &r_tld, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &v, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &w, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &wt,Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &d, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &s, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &z, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &q, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &p, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &pt,Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &y, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &yt, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &vt, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));
    CHECK( magma_dvinit( &zt, Magma_DEV, A.num_rows, b.num_cols, c_zero, queue ));

    
    // solver setup
    CHECK(  magma_dresidualvec( A, b, *x, &r, &nom0, queue));
    solver_par->init_res = nom0;
    magma_dcopy( dofs, r.dval, 1, r_tld.dval, 1, queue );   
    magma_dcopy( dofs, r.dval, 1, vt.dval, 1, queue );  
    magma_dcopy( dofs, r.dval, 1, wt.dval, 1, queue );   
     
    
    // transpose the matrix
    magma_dmtransfer( A, &Ah1, Magma_DEV, Magma_CPU, queue );
    magma_dmconvert( Ah1, &Ah2, A.storage_type, Magma_CSR, queue );
    magma_dmfree(&Ah1, queue );
    magma_dmtransposeconjugate( Ah2, &Ah1, queue );
    magma_dmfree(&Ah2, queue );
    Ah2.blocksize = A.blocksize;
    Ah2.alignment = A.alignment;
    magma_dmconvert( Ah1, &Ah2, Magma_CSR, A.storage_type, queue );
    magma_dmfree(&Ah1, queue );
    magma_dmtransfer( Ah2, &AT, Magma_CPU, Magma_DEV, queue );
    magma_dmfree(&Ah2, queue );
    
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
        // no precond: y = vt, z = wt
        // magma_dcopy( dofs, vt.dval, 1, y.dval, 1, queue );
        // magma_dcopy( dofs, wt.dval, 1, z.dval, 1, queue );
    CHECK( magma_d_applyprecond_left( MagmaNoTrans, A, vt, &y, precond_par, queue ));
    CHECK( magma_d_applyprecond_right( MagmaTrans, A, wt, &z, precond_par, queue ));

    psi = magma_dsqrt( magma_ddot( dofs, z.dval, 1, z.dval, 1, queue ));
    rho = magma_dsqrt( magma_ddot( dofs, y.dval, 1, y.dval, 1, queue ));
        // v = vt / rho
        // y = y / rho
        // w = wt / psi
        // z = z / psi
    magma_dcopy( dofs, vt.dval, 1, v.dval, 1, queue );  
    magma_dcopy( dofs, wt.dval, 1, w.dval, 1, queue );  
    magma_dscal( dofs, c_one / rho, v.dval, 1, queue ); 
    magma_dscal( dofs, c_one / rho, y.dval, 1, queue ); 
    magma_dscal( dofs, c_one / psi, w.dval, 1, queue ); 
    magma_dscal( dofs, c_one / psi, z.dval, 1, queue ); 

    //Chronometry
    real_Double_t tempo1, tempo2;
    tempo1 = magma_sync_wtime( queue );
    
    solver_par->numiter = 0;
    // start iteration
    do
    {
        solver_par->numiter++;
        if( magma_d_isnan_inf( rho ) || magma_d_isnan_inf( psi ) ){
            info = MAGMA_DIVERGENCE;
            break;
        }
            // delta = z' * y;
        delta = magma_ddot( dofs, z.dval, 1, y.dval, 1, queue );
        if( magma_d_isnan_inf( delta ) ){
            info = MAGMA_DIVERGENCE;
            break;
        }
            // no precond: yt = y, zt = z
        // magma_dcopy( dofs, y.dval, 1, yt.dval, 1, queue );
        // magma_dcopy( dofs, z.dval, 1, zt.dval, 1, queue );
        CHECK( magma_d_applyprecond_right( MagmaNoTrans, A, y, &yt, precond_par, queue ));
        CHECK( magma_d_applyprecond_left( MagmaTrans, A, z, &zt, precond_par, queue ));

        
        if( solver_par->numiter == 1 ){
                // p = y;
                // q = z;
            magma_dcopy( dofs, yt.dval, 1, p.dval, 1, queue );
            magma_dcopy( dofs, zt.dval, 1, q.dval, 1, queue );
        }
        else{
            pde = psi * delta / epsilon;
            rde = rho * MAGMA_D_CONJ(delta/epsilon);
                // p = yt - pde * p;
            magma_dscal( dofs, -pde, p.dval, 1, queue );    
            magma_daxpy( dofs, c_one, yt.dval, 1, p.dval, 1, queue );
                // q = zt - rde * q;
            magma_dscal( dofs, -rde, q.dval, 1, queue );    
            magma_daxpy( dofs, c_one, zt.dval, 1, q.dval, 1, queue );
        }
        if( magma_d_isnan_inf( rho ) || magma_d_isnan_inf( psi ) ){
            info = MAGMA_DIVERGENCE;
            break;
        }

        CHECK( magma_d_spmv( c_one, A, p, c_zero, pt, queue ));
        solver_par->spmv_count++;
            // epsilon = q' * pt;
        epsilon = magma_ddot( dofs, q.dval, 1, pt.dval, 1, queue );
        beta = epsilon / delta;

        if( magma_d_isnan_inf( epsilon ) || magma_d_isnan_inf( beta ) ){
            info = MAGMA_DIVERGENCE;
            break;
        }
            // vt = pt - beta * v;
        magma_dcopy( dofs, v.dval, 1, vt.dval, 1, queue );
        magma_dscal( dofs, -beta, vt.dval, 1, queue ); 
        magma_daxpy( dofs, c_one, pt.dval, 1, vt.dval, 1, queue ); 
            // no precond: y = v
        //magma_dcopy( dofs, v.dval, 1, y.dval, 1, queue );

            // wt = A' * q - beta' * w;
        CHECK( magma_d_spmv( c_one, AT, q, c_zero, wt, queue ));
        solver_par->spmv_count++;
        
        
        magma_daxpy( dofs, - MAGMA_D_CONJ( beta ), w.dval, 1, wt.dval, 1, queue );  
            // no precond: z = wt
        // magma_dcopy( dofs, wt.dval, 1, z.dval, 1, queue );
        CHECK( magma_d_applyprecond_right( MagmaTrans, A, wt, &z, precond_par, queue ));
        
        CHECK( magma_d_applyprecond_left( MagmaNoTrans, A, vt, &y, precond_par, queue ));

        rho1 = rho;      
            // rho = norm(y);
        rho = magma_dsqrt( magma_ddot( dofs, y.dval, 1, y.dval, 1, queue ));

        thet1 = thet;        
        thet = rho / (gamm * MAGMA_D_MAKE( MAGMA_D_ABS(beta), 0.0 ));
        gamm1 = gamm;        
        
        gamm = c_one / magma_dsqrt(c_one + thet*thet);        
        eta = - eta * rho1 * gamm * gamm / (beta * gamm1 * gamm1);        

        if( magma_d_isnan_inf( thet ) || magma_d_isnan_inf( gamm ) || magma_d_isnan_inf( eta ) ){
            info = MAGMA_DIVERGENCE;
            break;
        }
        if( solver_par->numiter == 1 ){
                // d = eta * p;
                // s = eta * pt;
            magma_dcopy( dofs, p.dval, 1, d.dval, 1, queue );
            magma_dscal( dofs, eta, d.dval, 1, queue );
            magma_dcopy( dofs, pt.dval, 1, s.dval, 1, queue );
            magma_dscal( dofs, eta, s.dval, 1, queue );
                // x = x + d;                    
            magma_daxpy( dofs, c_one, d.dval, 1, x->dval, 1, queue );
                // r = r - s;
            magma_daxpy( dofs, -c_one, s.dval, 1, r.dval, 1, queue );
        }
        else{
                // d = eta * p + (thet1 * gamm)^2 * d;
                // s = eta * pt + (thet1 * gamm)^2 * s;
            pds = (thet1 * gamm) * (thet1 * gamm);
            magma_dscal( dofs, pds, d.dval, 1, queue );    
            magma_daxpy( dofs, eta, p.dval, 1, d.dval, 1, queue );
            magma_dscal( dofs, pds, s.dval, 1, queue );    
            magma_daxpy( dofs, eta, pt.dval, 1, s.dval, 1, queue );
                // x = x + d;                    
            magma_daxpy( dofs, c_one, d.dval, 1, x->dval, 1, queue );
                // r = r - s;
            magma_daxpy( dofs, -c_one, s.dval, 1, r.dval, 1, queue );
        }
            // psi = norm(z);
        psi = magma_dsqrt( magma_ddot( dofs, z.dval, 1, z.dval, 1, queue ) );
        
        res = magma_dnrm2( dofs, r.dval, 1, queue );
        
        if ( solver_par->verbose > 0 ) {
            tempo2 = magma_sync_wtime( queue );
            if ( (solver_par->numiter)%solver_par->verbose == c_zero ) {
                solver_par->res_vec[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) res;
                solver_par->timing[(solver_par->numiter)/solver_par->verbose]
                        = (real_Double_t) tempo2-tempo1;
            }
        }

        // v = y / rho
        // y = y / rho
        // w = wt / psi
        // z = z / psi
        magma_dqmr_8(  
        r.num_rows, 
        r.num_cols, 
        rho,
        psi,
        vt.dval,
        wt.dval,
        y.dval, 
        z.dval,
        v.dval,
        w.dval,
        queue );

        if ( res/nomb <= solver_par->rtol || res <= solver_par->atol ){
            break;
        }
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
            if ( (solver_par->numiter)%solver_par->verbose == c_zero ) {
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
            if ( (solver_par->numiter)%solver_par->verbose == c_zero ) {
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
    magma_dmfree(&r_tld, queue );
    magma_dmfree(&v,  queue );
    magma_dmfree(&w,  queue );
    magma_dmfree(&wt, queue );
    magma_dmfree(&d,  queue );
    magma_dmfree(&s,  queue );
    magma_dmfree(&z,  queue );
    magma_dmfree(&q,  queue );
    magma_dmfree(&p,  queue );
    magma_dmfree(&zt, queue );
    magma_dmfree(&vt, queue );
    magma_dmfree(&yt, queue );
    magma_dmfree(&pt, queue );
    magma_dmfree(&y,  queue );
    magma_dmfree(&AT, queue );
    magma_dmfree(&Ah1, queue );
    magma_dmfree(&Ah2, queue );

    
    solver_par->info = info;
    return info;
}   /* magma_dqmr */
