/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Hartwig Anzt
       @author Eduardo Ponce
       @author Stephen Wood

       @generated from sparse/src/zpidr.cpp, normal z -> s, Sat Mar 27 20:32:59 2021
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
    This is a GPU implementation of the preconditioned Induced Dimension
    Reduction method.

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
magma_spidr(
    magma_s_matrix A, magma_s_matrix b, magma_s_matrix *x,
    magma_s_solver_par *solver_par,
    magma_s_preconditioner *precond_par,
    magma_queue_t queue )
{
    magma_int_t info = MAGMA_NOTCONVERGED;

    // prepare solver feedback
    solver_par->solver = Magma_PIDR;
    solver_par->numiter = 0;
    solver_par->spmv_count = 0;
    solver_par->init_res = 0.0;
    solver_par->final_res = 0.0;
    solver_par->iter_res = 0.0;
    solver_par->runtime = 0.0;

    // constants
    const float c_zero = MAGMA_S_ZERO;
    const float c_one = MAGMA_S_ONE;
    const float c_n_one = MAGMA_S_NEG_ONE;

    // internal user parameters
    const magma_int_t smoothing = 1;   // 0 = disable, 1 = enable
    const float angle = 0.7;          // [0-1]

    // local variables
    magma_int_t iseed[4] = {0, 0, 0, 1};
    magma_int_t dof;
    magma_int_t s;
    magma_int_t distr;
    magma_int_t k, i, sk;
    magma_int_t innerflag;
    float residual;
    float nrm;
    float nrmb;
    float nrmr;
    float nrmt;
    float rho;
    float om;
    float tt;
    float tr;
    float gamma;
    float alpha;
    float mkk;
    float fk;

    // matrices and vectors
    magma_s_matrix dxs = {Magma_CSR};
    magma_s_matrix dr = {Magma_CSR}, drs = {Magma_CSR};
    magma_s_matrix dP = {Magma_CSR}, dP1 = {Magma_CSR};
    magma_s_matrix dG = {Magma_CSR};
    magma_s_matrix dU = {Magma_CSR};
    magma_s_matrix dM = {Magma_CSR};
    magma_s_matrix df = {Magma_CSR};
    magma_s_matrix dt = {Magma_CSR};
    magma_s_matrix dc = {Magma_CSR};
    magma_s_matrix dv = {Magma_CSR};
    magma_s_matrix dvtmp = {Magma_CSR};
    magma_s_matrix dbeta = {Magma_CSR}, hbeta = {Magma_CSR};
    magma_s_matrix dlu = {Magma_CSR};

    // chronometry
    real_Double_t tempo1, tempo2;

    // initial s space
    // TODO: add option for 's' (shadow space number)
    // Hack: uses '--restart' option as the shadow space number.
    //       This is not a good idea because the default value of restart option is used to detect
    //       if the user provided a custom restart. This means that if the default restart value
    //       is changed then the code will think it was the user (unless the default value is
    //       also updated in the 'if' statement below.
    s = 1;
    if ( solver_par->restart != 50 ) {
        if ( solver_par->restart > A.num_cols ) {
            s = A.num_cols;
        } else {
            s = solver_par->restart;
        }
    }
    solver_par->restart = s;

    // set max iterations
    solver_par->maxiter = min( 2 * A.num_cols, solver_par->maxiter );

    // check if matrix A is square
    if ( A.num_rows != A.num_cols ) {
        //printf("Matrix A is not square.\n");
        info = MAGMA_ERR_NOT_SUPPORTED;
        goto cleanup;
    }

    // |b|
    nrmb = magma_snrm2( b.num_rows, b.dval, 1, queue );
    if ( nrmb == 0.0 ) {
        magma_sscal( x->num_rows, MAGMA_S_ZERO, x->dval, 1, queue );
        info = MAGMA_SUCCESS;
        goto cleanup;
    }

    // r = b - A x
    CHECK( magma_svinit( &dr, Magma_DEV, b.num_rows, 1, c_zero, queue ));
    CHECK( magma_sresidualvec( A, b, *x, &dr, &nrmr, queue ));
    
    // |r|
    solver_par->init_res = nrmr;
    solver_par->final_res = solver_par->init_res;
    solver_par->iter_res = solver_par->init_res;
    if ( solver_par->verbose > 0 ) {
        solver_par->res_vec[0] = (real_Double_t)nrmr;
    }

    // check if initial is guess good enough
    if ( nrmr <= solver_par->atol ||
        nrmr/nrmb <= solver_par->rtol ) {
        info = MAGMA_SUCCESS;
        goto cleanup;
    }

    // P = randn(n, s)
    // P = ortho(P)
//---------------------------------------
    // P = 0.0
    CHECK( magma_svinit( &dP, Magma_CPU, A.num_cols, s, c_zero, queue ));

    // P = randn(n, s)
    distr = 3;        // 1 = unif (0,1), 2 = unif (-1,1), 3 = normal (0,1) 
    dof = dP.num_rows * dP.num_cols;
    lapackf77_slarnv( &distr, iseed, &dof, dP.val );

    // transfer P to device
    CHECK( magma_smtransfer( dP, &dP1, Magma_CPU, Magma_DEV, queue ));
    magma_smfree( &dP, queue );

    // P = ortho(P1)
    if ( dP1.num_cols > 1 ) {
        // P = magma_sqr(P1), QR factorization
        CHECK( magma_sqr( dP1.num_rows, dP1.num_cols, dP1, dP1.ld, &dP, NULL, queue ));
    } else {
        // P = P1 / |P1|
        nrm = magma_snrm2( dof, dP1.dval, 1, queue );
        nrm = 1.0 / nrm;
        magma_sscal( dof, nrm, dP1.dval, 1, queue );
        CHECK( magma_smtransfer( dP1, &dP, Magma_DEV, Magma_DEV, queue ));
    }
    magma_smfree( &dP1, queue );
//---------------------------------------

    // allocate memory for the scalar products
    CHECK( magma_svinit( &hbeta, Magma_CPU, s, 1, c_zero, queue ));
    CHECK( magma_svinit( &dbeta, Magma_DEV, s, 1, c_zero, queue ));

    // smoothing enabled
    if ( smoothing > 0 ) {
        // set smoothing solution vector
        CHECK( magma_smtransfer( *x, &dxs, Magma_DEV, Magma_DEV, queue ));

        // set smoothing residual vector
        CHECK( magma_smtransfer( dr, &drs, Magma_DEV, Magma_DEV, queue ));
    }

    // G(n,s) = 0
    CHECK( magma_svinit( &dG, Magma_DEV, A.num_cols, s, c_zero, queue ));

    // U(n,s) = 0
    CHECK( magma_svinit( &dU, Magma_DEV, A.num_cols, s, c_zero, queue ));

    // M(s,s) = I
    CHECK( magma_svinit( &dM, Magma_DEV, s, s, c_zero, queue ));
    magmablas_slaset( MagmaFull, s, s, c_zero, c_one, dM.dval, s, queue );

    // f = 0
    CHECK( magma_svinit( &df, Magma_DEV, dP.num_cols, 1, c_zero, queue ));

    // t = 0
    CHECK( magma_svinit( &dt, Magma_DEV, dr.num_rows, 1, c_zero, queue ));

    // c = 0
    CHECK( magma_svinit( &dc, Magma_DEV, dM.num_cols, 1, c_zero, queue ));

    // v = 0
    CHECK( magma_svinit( &dv, Magma_DEV, dr.num_rows, 1, c_zero, queue ));
    CHECK( magma_svinit( &dvtmp, Magma_DEV, dr.num_rows, 1, c_zero, queue ));

    // lu = 0
    CHECK( magma_svinit( &dlu, Magma_DEV, A.num_rows, 1, c_zero, queue ));

    //--------------START TIME---------------
    // chronometry
    tempo1 = magma_sync_wtime( queue );
    if ( solver_par->verbose > 0 ) {
        solver_par->timing[0] = 0.0;
    }

    om = MAGMA_S_ONE;
    innerflag = 0;

    // start iteration
    do
    {
        solver_par->numiter++;
    
        // new RHS for small systems
        // f = P' r
        magmablas_sgemv( MagmaConjTrans, dP.num_rows, dP.num_cols, c_one, dP.dval, dP.ld, dr.dval, 1, c_zero, df.dval, 1, queue );

        // shadow space loop
        for ( k = 0; k < s; ++k ) {
            sk = s - k;
    
            // f(k:s) = M(k:s,k:s) c(k:s)
            magma_scopyvector( sk, &df.dval[k], 1, &dc.dval[k], 1, queue );
            magma_strsv( MagmaLower, MagmaNoTrans, MagmaNonUnit, sk, &dM.dval[k*dM.ld+k], dM.ld, &dc.dval[k], 1, queue );

            // v = r - G(:,k:s) c(k:s)
            magma_scopyvector( dr.num_rows, dr.dval, 1, dv.dval, 1, queue );
            magmablas_sgemv( MagmaNoTrans, dG.num_rows, sk, c_n_one, &dG.dval[k*dG.ld], dG.ld, &dc.dval[k], 1, c_one, dv.dval, 1, queue );

            // preconditioning operation 
            // v = L \ v;
            // v = U \ v;
            CHECK( magma_s_applyprecond_left( MagmaNoTrans, A, dv, &dlu, precond_par, queue )); 
            CHECK( magma_s_applyprecond_right( MagmaNoTrans, A, dlu, &dv, precond_par, queue )); 

            // U(:,k) = om * v + U(:,k:s) c(k:s)
            magmablas_sgemv( MagmaNoTrans, dU.num_rows, sk, c_one, &dU.dval[k*dU.ld], dU.ld, &dc.dval[k], 1, om, dv.dval, 1, queue );
            magma_scopyvector( dU.num_rows, dv.dval, 1, &dU.dval[k*dU.ld], 1, queue );
            magma_scopyvector( dU.num_rows, dv.dval, 1, dvtmp.dval, 1, queue );

            // G(:,k) = A U(:,k)
            CHECK( magma_s_spmv( c_one, A, dvtmp, c_zero, dv, queue ));
            solver_par->spmv_count++;
            magma_scopyvector( dG.num_rows, dv.dval, 1, &dG.dval[k*dG.ld], 1, queue );

            // bi-orthogonalize the new basis vectors
            for ( i = 0; i < k; ++i ) {
                // alpha = P(:,i)' G(:,k)
                alpha = magma_sdot( dP.num_rows, &dP.dval[i*dP.ld], 1, &dG.dval[k*dG.ld], 1, queue );

                // alpha = alpha / M(i,i)
                magma_sgetvector( 1, &dM.dval[i*dM.ld+i], 1, &mkk, 1, queue );
                alpha = alpha / mkk;

                // G(:,k) = G(:,k) - alpha * G(:,i)
                magma_saxpy( dG.num_rows, -alpha, &dG.dval[i*dG.ld], 1, &dG.dval[k*dG.ld], 1, queue );

                // U(:,k) = U(:,k) - alpha * U(:,i)
                magma_saxpy( dU.num_rows, -alpha, &dU.dval[i*dU.ld], 1, &dU.dval[k*dU.ld], 1, queue );
            }

            // new column of M = P'G, first k-1 entries are zero
            // M(k:s,k) = P(:,k:s)' G(:,k)
            magmablas_sgemv( MagmaConjTrans, dP.num_rows, sk, c_one, &dP.dval[k*dP.ld], dP.ld, &dG.dval[k*dG.ld], 1, c_zero, &dM.dval[k*dM.ld+k], 1, queue );

            // check M(k,k) == 0
            magma_sgetvector( 1, &dM.dval[k*dM.ld+k], 1, &mkk, 1, queue );
            if ( MAGMA_S_EQUAL(mkk, MAGMA_S_ZERO) ) {
                innerflag = 1;
                info = MAGMA_DIVERGENCE;
                break;
            }

            // beta = f(k) / M(k,k)
            magma_sgetvector( 1, &df.dval[k], 1, &fk, 1, queue );
            hbeta.val[k] = fk / mkk;

            // check for nan
            if ( magma_s_isnan( hbeta.val[k] ) || magma_s_isinf( hbeta.val[k] )) {
                innerflag = 1;
                info = MAGMA_DIVERGENCE;
                break;
            }

            // r = r - beta * G(:,k)
            magma_saxpy( dr.num_rows, -hbeta.val[k], &dG.dval[k*dG.ld], 1, dr.dval, 1, queue );

            // smoothing disabled
            if ( smoothing <= 0 ) {
                // |r|
                nrmr = magma_snrm2( dr.num_rows, dr.dval, 1, queue );

            // smoothing enabled
            } else {
                // x = x + beta * U(:,k)
                magma_saxpy( x->num_rows, hbeta.val[k], &dU.dval[k*dU.ld], 1, x->dval, 1, queue );

                // smoothing operation
//---------------------------------------
                // t = rs - r
                magma_scopyvector( drs.num_rows, drs.dval, 1, dt.dval, 1, queue );
                magma_saxpy( dt.num_rows, c_n_one, dr.dval, 1, dt.dval, 1, queue );

                // t't
                // t'rs 
                tt = magma_sdot( dt.num_rows, dt.dval, 1, dt.dval, 1, queue );
                tr = magma_sdot( dt.num_rows, dt.dval, 1, drs.dval, 1, queue );

                // gamma = (t' * rs) / (t' * t)
                gamma = tr / tt;

                // rs = rs - gamma * (rs - r) 
                magma_saxpy( drs.num_rows, -gamma, dt.dval, 1, drs.dval, 1, queue );

                // xs = xs - gamma * (xs - x) 
                magma_scopyvector( dxs.num_rows, dxs.dval, 1, dt.dval, 1, queue );
                magma_saxpy( dt.num_rows, c_n_one, x->dval, 1, dt.dval, 1, queue );
                magma_saxpy( dxs.num_rows, -gamma, dt.dval, 1, dxs.dval, 1, queue );

                // |rs|
                nrmr = magma_snrm2( drs.num_rows, drs.dval, 1, queue );           
//---------------------------------------
            }

            // store current timing and residual
            if ( solver_par->verbose > 0 ) {
                tempo2 = magma_sync_wtime( queue );
                if ( (solver_par->numiter) % solver_par->verbose == 0 ) {
                    solver_par->res_vec[(solver_par->numiter) / solver_par->verbose]
                            = (real_Double_t)nrmr;
                    solver_par->timing[(solver_par->numiter) / solver_par->verbose]
                            = (real_Double_t)tempo2 - tempo1;
                }
            }

            // check convergence
            if ( nrmr <= solver_par->atol ||
                nrmr/nrmb <= solver_par->rtol ) {
                s = k + 1; // for the x-update outside the loop
                innerflag = 2;
                info = MAGMA_SUCCESS;
                break;
            }

            // non-last s iteration
            if ( (k + 1) < s ) {
                // f(k+1:s) = f(k+1:s) - beta * M(k+1:s,k)
                magma_saxpy( sk-1, -hbeta.val[k], &dM.dval[k*dM.ld+(k+1)], 1, &df.dval[k+1], 1, queue );
            }
        }

        // smoothing disabled
        if ( smoothing <= 0 && innerflag != 1 ) {
            // update solution approximation x
            // x = x + U(:,1:s) * beta(1:s)
            magma_ssetvector( s, hbeta.val, 1, dbeta.dval, 1, queue );
            magmablas_sgemv( MagmaNoTrans, dU.num_rows, s, c_one, dU.dval, dU.ld, dbeta.dval, 1, c_one, x->dval, 1, queue );
        }

        // check convergence or iteration limit or invalid result of inner loop
        if ( innerflag > 0 ) {
            break;
        }

        // v = r
        magma_scopyvector( dr.num_rows, dr.dval, 1, dv.dval, 1, queue );

        // preconditioning operation 
        // v = L \ v;
        // v = U \ v;
        CHECK( magma_s_applyprecond_left( MagmaNoTrans, A, dv, &dlu, precond_par, queue )); 
        CHECK( magma_s_applyprecond_right( MagmaNoTrans, A, dlu, &dv, precond_par, queue )); 

        // t = A v
        CHECK( magma_s_spmv( c_one, A, dv, c_zero, dt, queue ));
        solver_par->spmv_count++;

        // computation of a new omega
//---------------------------------------
        // |t|
        nrmt = magma_snrm2( dt.num_rows, dt.dval, 1, queue );

        // t'r 
        tr = magma_sdot( dt.num_rows, dt.dval, 1, dr.dval, 1, queue );

        // rho = abs(t' * r) / (|t| * |r|))
        rho = MAGMA_D_ABS( MAGMA_S_REAL(tr) / (nrmt * nrmr) );

        // om = (t' * r) / (|t| * |t|)
        om = tr / (nrmt * nrmt);
        if ( rho < angle ) {
            om = (om * angle) / rho;
        }
//---------------------------------------
        if ( MAGMA_S_EQUAL(om, MAGMA_S_ZERO) ) {
            info = MAGMA_DIVERGENCE;
            break;
        }

        // update approximation vector
        // x = x + om * v
        magma_saxpy( x->num_rows, om, dv.dval, 1, x->dval, 1, queue );

        // update residual vector
        // r = r - om * t
        magma_saxpy( dr.num_rows, -om, dt.dval, 1, dr.dval, 1, queue );

        // smoothing disabled
        if ( smoothing <= 0 ) {
            // residual norm
            nrmr = magma_snrm2( b.num_rows, dr.dval, 1, queue );

        // smoothing enabled
        } else {
            // smoothing operation
//---------------------------------------
            // t = rs - r
            magma_scopyvector( drs.num_rows, drs.dval, 1, dt.dval, 1, queue );
            magma_saxpy( dt.num_rows, c_n_one, dr.dval, 1, dt.dval, 1, queue );

            // t't
            // t'rs
            tt = magma_sdot( dt.num_rows, dt.dval, 1, dt.dval, 1, queue );
            tr = magma_sdot( dt.num_rows, dt.dval, 1, drs.dval, 1, queue );

            // gamma = (t' * rs) / (|t| * |t|)
            gamma = tr / tt;

            // rs = rs - gamma * (rs - r) 
            magma_saxpy( drs.num_rows, -gamma, dt.dval, 1, drs.dval, 1, queue );

            // xs = xs - gamma * (xs - x) 
            magma_scopyvector( dxs.num_rows, dxs.dval, 1, dt.dval, 1, queue );
            magma_saxpy( dt.num_rows, c_n_one, x->dval, 1, dt.dval, 1, queue );
            magma_saxpy( dxs.num_rows, -gamma, dt.dval, 1, dxs.dval, 1, queue );

            // |rs|
            nrmr = magma_snrm2( b.num_rows, drs.dval, 1, queue );           
//---------------------------------------
        }

        // store current timing and residual
        if ( solver_par->verbose > 0 ) {
            tempo2 = magma_sync_wtime( queue );
            if ( (solver_par->numiter) % solver_par->verbose == 0 ) {
                solver_par->res_vec[(solver_par->numiter) / solver_par->verbose]
                        = (real_Double_t)nrmr;
                solver_par->timing[(solver_par->numiter) / solver_par->verbose]
                        = (real_Double_t)tempo2 - tempo1;
            }
        }

        // check convergence
        if ( nrmr <= solver_par->atol ||
            nrmr/nrmb <= solver_par->rtol ) { 
            info = MAGMA_SUCCESS;
            break;
        }
    }
    while ( solver_par->numiter + 1 <= solver_par->maxiter );

    // smoothing enabled
    if ( smoothing > 0 ) {
        // x = xs
        magma_scopyvector( x->num_rows, dxs.dval, 1, x->dval, 1, queue );

        // r = rs
        magma_scopyvector( dr.num_rows, drs.dval, 1, dr.dval, 1, queue );
    }

    // get last iteration timing
    tempo2 = magma_sync_wtime( queue );
    solver_par->runtime = (real_Double_t)tempo2 - tempo1;
//--------------STOP TIME----------------

    // get final stats
    solver_par->iter_res = nrmr;
    CHECK( magma_sresidualvec( A, b, *x, &dr, &residual, queue ));
    solver_par->final_res = residual;

    // set solver conclusion
    if ( info != MAGMA_SUCCESS && info != MAGMA_DIVERGENCE ) {
        if ( solver_par->init_res > solver_par->final_res ) {
            info = MAGMA_SLOW_CONVERGENCE;
        }
    }


cleanup:
    // free resources
    // smoothing enabled
    if ( smoothing > 0 ) {
        magma_smfree( &dxs, queue );
        magma_smfree( &drs, queue );
    }
    magma_smfree( &dr, queue );
    magma_smfree( &dP, queue );
    magma_smfree( &dP1, queue );
    magma_smfree( &dG, queue );
    magma_smfree( &dU, queue );
    magma_smfree( &dM, queue );
    magma_smfree( &df, queue );
    magma_smfree( &dt, queue );
    magma_smfree( &dc, queue );
    magma_smfree( &dv, queue );
    magma_smfree( &dvtmp, queue );
    magma_smfree( &dlu, queue);
    magma_smfree( &dbeta, queue );
    magma_smfree( &hbeta, queue );

    solver_par->info = info;
    return info;
    /* magma_spidr */
}
