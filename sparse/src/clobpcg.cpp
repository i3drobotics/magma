/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver

       @date
            
       @author Stan Tomov
       @author Hartwig Anzt

       @generated from sparse/src/zlobpcg.cpp, normal z -> c, Sat Mar 27 20:33:01 2021
*/
#include "magmasparse_internal.h"

#define PRECISION_c
#define COMPLEX

#define RTOLERANCE     lapackf77_slamch( "E" )
#define ATOLERANCE     lapackf77_slamch( "E" )


/**
    Purpose
    -------
    Solves an eigenvalue problem

       A * X = evalues X

    where A is a complex sparse matrix stored in the GPU memory.
    X and B are complex vectors stored on the GPU memory.

    This is a GPU implementation of the LOBPCG method.
    
    This method allocates all required memory space inside the routine.
    Also, the memory is not allocated as one big chunk, but seperatly for
    the different blocks. This allows to use texture also for large matrices.

    Arguments
    ---------
    @param[in]
    A           magma_c_matrix
                input matrix A

    @param[in,out]
    solver_par  magma_c_solver_par*
                solver parameters

    @param[in,out]
    precond_par magma_c_precond_par*
                preconditioner parameters
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_cheev
    ********************************************************************/

extern "C" magma_int_t
magma_clobpcg(
    magma_c_matrix A,
    magma_c_solver_par *solver_par,
    magma_c_preconditioner *precond_par,
    magma_queue_t queue )
{
    magma_int_t info = 0;
        
    #define  residualNorms(i,iter)  ( residualNorms + (i) + (iter)*n )
    #define SWAP(x, y)    { pointer = x; x = y; y = pointer; }
    #define hresidualNorms(i,iter)  (hresidualNorms + (i) + (iter)*n )
    
    #define gramA(    m, n)   (gramA     + (m) + (n)*ldgram)
    #define gramB(    m, n)   (gramB     + (m) + (n)*ldgram)
    #define gevectors(m, n)   (gevectors + (m) + (n)*ldgram)
    #define h_gramB(  m, n)   (h_gramB   + (m) + (n)*ldgram)



    #define magma_c_bspmv_tuned(m, n, alpha, A, X, beta, AX, queue)       {        \
                magma_c_matrix x={Magma_CSR}, ax={Magma_CSR};                                       \
                x.memory_location = Magma_DEV;  x.num_rows = m;  x.num_cols = n;  x.major = MagmaColMajor;   x.nnz = m*n;  x.dval = X;    x.storage_type = Magma_DENSE; \
                ax.memory_location= Magma_DEV; ax.num_rows = m; ax.num_cols = n; ax.major = MagmaColMajor;  ax.nnz = m*n; ax.dval = AX;  ax.storage_type = Magma_DENSE; \
                CHECK( magma_c_spmv(alpha, A, x, beta, ax, queue ));                   \
    }



    //**************************************************************
    // %Memory allocation for the eigenvectors, eigenvalues, and workspace
    solver_par->solver = Magma_LOBPCG;
    magma_int_t m = A.num_rows;
    magma_int_t n = (solver_par->num_eigenvalues);
    magmaFloatComplex *blockX = solver_par->eigenvectors;
    float *evalues = solver_par->eigenvalues;
    solver_par->numiter = 0;
    solver_par->spmv_count = 0;


    magmaFloatComplex *dwork=NULL, *hwork=NULL;
    magmaFloatComplex *blockP=NULL, *blockAP=NULL, *blockR=NULL, *blockAR=NULL, *blockAX=NULL, *blockW=NULL;
    magmaFloatComplex *gramA=NULL, *gramB=NULL, *gramM=NULL;
    magmaFloatComplex *gevectors=NULL, *h_gramB=NULL;
    
    dwork = NULL;
    hwork = NULL;
    blockP = NULL;
    blockR = NULL;
    blockAP = NULL;
    blockAR = NULL;
    blockAX = NULL;
    blockW = NULL;
    gramA = NULL;
    gramB = NULL;
    gramM = NULL;
    gevectors = NULL;
    h_gramB = NULL;

    magmaFloatComplex *pointer, *origX = blockX;
    float *eval_gpu=NULL;
    
    magma_int_t iterationNumber, cBlockSize, restart = 1, iter;

    //Chronometry
    real_Double_t tempo1, tempo2;
    
    magma_int_t lwork = max( 2*n+n*magma_get_dsytrd_nb(n),
                                            1 + 6*3*n + 2* 3*n* 3*n);
    
    magma_int_t *iwork={0}, liwork = 15*n+9;
    magma_int_t gramDim, ldgram  = 3*n, ikind = 3;
    
    magmaFloatComplex *hW={0};

    // === Set solver parameters ===
    float residualTolerance  = solver_par->rtol;
    magma_int_t maxIterations = solver_par->maxiter;
    float tmp;
    float r0=0;  // set in 1st iteration

    // === Set some constants & defaults ===
    magmaFloatComplex c_zero = MAGMA_C_ZERO;
    magmaFloatComplex c_one  = MAGMA_C_ONE;
    magmaFloatComplex c_neg_one = MAGMA_C_NEG_ONE;
    
    float *residualNorms={0}, *condestGhistory={0}, condestG={0};
    float *gevalues={0};
    magma_int_t *activeMask={0};
    float *hresidualNorms={0};
    
#ifdef COMPLEX
    float *rwork={0};
    magma_int_t lrwork = 1 + 5*(3*n) + 2*(3*n)*(3*n);

    CHECK( magma_smalloc_cpu(&rwork, lrwork));
#endif

    CHECK( magma_cmalloc_pinned( &hwork, lwork ));
    CHECK( magma_cmalloc( &blockAX,  m*n ));
    CHECK( magma_cmalloc( &blockAR,  m*n ));
    CHECK( magma_cmalloc( &blockAP,  m*n ));
    CHECK( magma_cmalloc( &blockR,   m*n ));
    CHECK( magma_cmalloc( &blockP,   m*n ));
    CHECK( magma_cmalloc( &blockW,   m*n ));
    CHECK( magma_cmalloc( &dwork,    m*n ));
    CHECK( magma_smalloc( &eval_gpu, 3*n ));

    //**********************************************************+
    // === Check some parameters for possible quick exit ===
    solver_par->info = MAGMA_SUCCESS;
    if (m < 2)
        info = MAGMA_DIVERGENCE;
    else if (n > m)
        info = MAGMA_SLOW_CONVERGENCE;

    if (solver_par->info != 0) {
        magma_xerbla( __func__, -(info) );
        goto cleanup;
    }
    solver_par->info = info; // local info variable;

    // === Allocate GPU memory for the residual norms' history ===
    CHECK( magma_smalloc(&residualNorms, (maxIterations+1) * n));
    CHECK( magma_malloc( (void **)&activeMask, (n+1) * sizeof(magma_int_t) ));

    // === Allocate CPU work space ===
    CHECK( magma_smalloc_cpu(&condestGhistory, maxIterations+1));
    CHECK( magma_smalloc_cpu(&gevalues, 3 * n));
    CHECK( magma_malloc_cpu((void **)&iwork, liwork * sizeof(magma_int_t)));


    CHECK( magma_cmalloc_pinned(&hW, n*n));
    CHECK( magma_cmalloc_pinned(&gevectors, 9*n*n));
    CHECK( magma_cmalloc_pinned(&h_gramB,   9*n*n));

    // === Allocate GPU workspace ===
    CHECK( magma_cmalloc(&gramM, n * n));
    CHECK( magma_cmalloc(&gramA, 9 * n * n));
    CHECK( magma_cmalloc(&gramB, 9 * n * n));



    // === Set activemask to one ===
    for(magma_int_t k =0; k<n; k++){
        iwork[k]=1;
    }
    magma_setmatrix(n, 1, sizeof(magma_int_t), iwork, n, activeMask, n, queue);

    #if defined(PRECISION_s)
    ikind = 3;
    #endif
    
    // === Make the initial vectors orthonormal ===
    magma_cgegqr_gpu(ikind, m, n, blockX, m, dwork, hwork, &info );

    //magma_corthomgs( m, n, blockX, queue );
    
    magma_c_bspmv_tuned(m, n, c_one, A, blockX, c_zero, blockAX, queue );
    solver_par->spmv_count++;
    // === Compute the Gram matrix = (X, AX) & its eigenstates ===
    magma_cgemm( MagmaConjTrans, MagmaNoTrans, n, n, m,
                c_one,  blockX, m, blockAX, m, c_zero, gramM, n, queue );

    magma_cheevd_gpu( MagmaVec, MagmaUpper,
                      n, gramM, n, evalues, hW, n, hwork, lwork,
                      #ifdef COMPLEX
                      rwork, lrwork,
                      #endif
                      iwork, liwork, &info );

    // === Update  X =  X * evectors ===
    magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, n, n,
                c_one,  blockX, m, gramM, n, c_zero, blockW, m, queue );
    SWAP(blockW, blockX);

    // === Update AX = AX * evectors ===
    magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, n, n,
                c_one,  blockAX, m, gramM, n, c_zero, blockW, m, queue );
    SWAP(blockW, blockAX);

    condestGhistory[1] = 7.82;


    tempo1 = magma_sync_wtime( queue );
    // === Main LOBPCG loop ============================================================
    for(iterationNumber = 1; iterationNumber < maxIterations; iterationNumber++)
        {
            // === compute the residuals (R = Ax - x evalues )
            magmablas_clacpy( MagmaFull, m, n, blockAX, m, blockR, m, queue );

            /*
            for(magma_int_t i=0; i<n; i++) {
                magma_caxpy( m, MAGMA_C_MAKE(-evalues[i],0), blockX+i*m, 1, blockR+i*m, 1, queue );
            }
            */
            magma_ssetmatrix( 3*n, 1, evalues, 3*n, eval_gpu, 3*n, queue );

            CHECK( magma_clobpcg_res( m, n, eval_gpu, blockX, blockR, eval_gpu, queue ));

            magmablas_scnrm2_cols( m, n, blockR, m, residualNorms(0, iterationNumber), queue );

            // === remove the residuals corresponding to already converged evectors
            CHECK( magma_ccompact(m, n, blockR, m,
                           residualNorms(0, iterationNumber), residualTolerance,
                           activeMask, &cBlockSize, queue ));
        
            if (cBlockSize == 0)
                break;

            // === apply a preconditioner P to the active residulas: R_new = P R_old
            // === for now set P to be identity (no preconditioner => nothing to be done )
            //magmablas_clacpy( MagmaFull, m, cBlockSize, blockR, m, blockW, m, queue );
            //SWAP(blockW, blockR);
            
            // preconditioner
            magma_c_matrix bWv={Magma_CSR}, bRv={Magma_CSR};
            bWv.memory_location = Magma_DEV;  bWv.num_rows = m; bWv.num_cols = cBlockSize; bWv.major = MagmaColMajor;  bWv.nnz = m*cBlockSize;  bWv.dval = blockW;
            bRv.memory_location = Magma_DEV;  bRv.num_rows = m; bRv.num_cols = cBlockSize; bRv.major = MagmaColMajor;  bRv.nnz = m*cBlockSize;  bRv.dval = blockR;
            CHECK( magma_c_applyprecond_left( MagmaNoTrans, A, bRv, &bWv, precond_par, queue ));
            CHECK( magma_c_applyprecond_right( MagmaNoTrans, A, bWv, &bRv, precond_par, queue ));
        
            // === make the preconditioned residuals orthogonal to X
            if( precond_par->solver != Magma_NONE){
                magma_cgemm( MagmaConjTrans, MagmaNoTrans, n, cBlockSize, m,
                            c_one, blockX, m, blockR, m, c_zero, gramB(0,0), ldgram, queue );
                magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, cBlockSize, n,
                            c_neg_one, blockX, m, gramB(0,0), ldgram, c_one, blockR, m, queue );
            }

            // === make the active preconditioned residuals orthonormal

            magma_cgegqr_gpu(ikind, m, cBlockSize, blockR, m, dwork, hwork, &info );
            #if defined(PRECISION_s)
            // re-orthogonalization
            SWAP(blockX, dwork);
            magma_cgegqr_gpu(ikind, m, cBlockSize, blockR, m, dwork, hwork, &info );
            #endif
            //magma_corthomgs( m, cBlockSize, blockR, queue );

            // === compute AR
            magma_c_bspmv_tuned(m, cBlockSize, c_one, A, blockR, c_zero, blockAR, queue );
            solver_par->spmv_count++;
            if (!restart) {
                // === compact P & AP as well
                CHECK( magma_ccompactActive(m, n, blockP,  m, activeMask, queue ));
                CHECK( magma_ccompactActive(m, n, blockAP, m, activeMask, queue ));
          
                /*
                // === make P orthogonal to X ?
                magma_cgemm( MagmaConjTrans, MagmaNoTrans, n, cBlockSize, m,
                            c_one, blockX, m, blockP, m, c_zero, gramB(0,0), ldgram, queue );
                magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, cBlockSize, n,
                            c_neg_one, blockX, m, gramB(0,0), ldgram, c_one, blockP, m, queue );

                // === make P orthogonal to R ?
                magma_cgemm( MagmaConjTrans, MagmaNoTrans, cBlockSize, cBlockSize, m,
                            c_one, blockR, m, blockP, m, c_zero, gramB(0,0), ldgram, queue );
                magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, cBlockSize, cBlockSize,
                            c_neg_one, blockR, m, gramB(0,0), ldgram, c_one, blockP, m, queue );
                */

                // === Make P orthonormal & properly change AP (without multiplication by A)
                magma_cgegqr_gpu(ikind, m, cBlockSize, blockP, m, dwork, hwork, &info );
                #if defined(PRECISION_s)
                // re-orthogonalization
                SWAP(blockX, dwork);
                magma_cgegqr_gpu(ikind, m, cBlockSize, blockP, m, dwork, hwork, &info );
                #endif
                //magma_corthomgs( m, cBlockSize, blockP, queue );

                //magma_c_bspmv_tuned(m, cBlockSize, c_one, A, blockP, c_zero, blockAP, queue );
                magma_csetmatrix( cBlockSize, cBlockSize, hwork, cBlockSize, dwork, cBlockSize, queue );

                // replacement according to Stan
                #if defined(PRECISION_s) || defined(PRECISION_d)
                magmablas_ctrsm( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaNonUnit,
                                 m, cBlockSize, c_one, dwork, cBlockSize, blockAP, m, queue );
                #else
                magma_ctrsm(     MagmaRight, MagmaUpper, MagmaNoTrans, MagmaNonUnit,
                                 m, cBlockSize, c_one, dwork, cBlockSize, blockAP, m, queue );
                #endif
            }

            iter = max( 1, iterationNumber - 10 - int(log(1.*cBlockSize)) );
            float condestGmean = 0.;
            for(magma_int_t i = 0; i<iterationNumber-iter+1; i++){
                condestGmean += condestGhistory[i];
            }
            condestGmean = condestGmean / (iterationNumber-iter+1);

            if (restart)
                gramDim = n+cBlockSize;
            else
                gramDim = n+2*cBlockSize;

            /* --- The Raileight-Ritz method for [X R P] -----------------------
               [ X R P ]'  [AX  AR  AP] y = evalues [ X R P ]' [ X R P ], i.e.,
       
                      GramA                                 GramB
                / X'AX  X'AR  X'AP \                 / X'X  X'R  X'P \
               |  R'AX  R'AR  R'AP  | y   = evalues |  R'X  R'R  R'P  |
                \ P'AX  P'AR  P'AP /                 \ P'X  P'R  P'P /
               -----------------------------------------------------------------   */

            // === assemble GramB; first, set it to I
            magmablas_claset( MagmaFull, ldgram, ldgram, c_zero, c_one, gramB, ldgram, queue );  // identity

            if (!restart) {
                magma_cgemm( MagmaConjTrans, MagmaNoTrans, cBlockSize, n, m,
                            c_one, blockP, m, blockX, m, c_zero, gramB(n+cBlockSize,0), ldgram, queue );
                magma_cgemm( MagmaConjTrans, MagmaNoTrans, cBlockSize, cBlockSize, m,
                            c_one, blockP, m, blockR, m, c_zero, gramB(n+cBlockSize,n), ldgram, queue );
            }
            magma_cgemm( MagmaConjTrans, MagmaNoTrans, cBlockSize, n, m,
                        c_one, blockR, m, blockX, m, c_zero, gramB(n,0), ldgram, queue );

            // === get GramB from the GPU to the CPU and compute its eigenvalues only
            magma_cgetmatrix( gramDim, gramDim, gramB, ldgram, h_gramB, ldgram, queue );
            lapackf77_cheev("N", "L", &gramDim, h_gramB, &ldgram, gevalues,
                            hwork, &lwork,
                            #ifdef COMPLEX
                            rwork,
                            #endif
                            &info);

            // === check stability criteria if we need to restart
            condestG = log10( gevalues[gramDim-1]/gevalues[0] ) + 1.;
            if ((condestG/condestGmean>2 && condestG>2) || condestG>8) {
                // Steepest descent restart for stability
                restart=1;
                printf("restart at step #%d\n", int(iterationNumber));
            }

            // === assemble GramA; first, set it to I
            magmablas_claset( MagmaFull, ldgram, ldgram, c_zero, c_one, gramA, ldgram, queue );  // identity

            magma_cgemm( MagmaConjTrans, MagmaNoTrans, cBlockSize, n, m,
                        c_one, blockR, m, blockAX, m, c_zero, gramA(n,0), ldgram, queue );
            magma_cgemm( MagmaConjTrans, MagmaNoTrans, cBlockSize, cBlockSize, m,
                        c_one, blockR, m, blockAR, m, c_zero, gramA(n,n), ldgram, queue );

            if (!restart) {
                magma_cgemm( MagmaConjTrans, MagmaNoTrans, cBlockSize, n, m,
                            c_one, blockP, m, blockAX, m, c_zero,
                            gramA(n+cBlockSize,0), ldgram, queue );
                magma_cgemm( MagmaConjTrans, MagmaNoTrans, cBlockSize, cBlockSize, m,
                            c_one, blockP, m, blockAR, m, c_zero,
                            gramA(n+cBlockSize,n), ldgram, queue );
                magma_cgemm( MagmaConjTrans, MagmaNoTrans, cBlockSize, cBlockSize, m,
                            c_one, blockP, m, blockAP, m, c_zero,
                            gramA(n+cBlockSize,n+cBlockSize), ldgram, queue );
            }

            /*
            // === Compute X' AX or just use the eigenvalues below ?
            magma_cgemm( MagmaConjTrans, MagmaNoTrans, n, n, m,
                        c_one, blockX, m, blockAX, m, c_zero,
                        gramA(0,0), ldgram, queue );
            */

            if (restart==0) {
                magma_cgetmatrix( gramDim, gramDim, gramA, ldgram, gevectors, ldgram, queue );
            }
            else {
                gramDim = n+cBlockSize;
                magma_cgetmatrix( gramDim, gramDim, gramA, ldgram, gevectors, ldgram, queue );
            }

            for(magma_int_t k=0; k<n; k++)
                *gevectors(k,k) = MAGMA_C_MAKE(evalues[k], 0);

            // === the previous eigensolver destroyed what is in h_gramB => must copy it again
            magma_cgetmatrix( gramDim, gramDim, gramB, ldgram, h_gramB, ldgram, queue );

            magma_int_t itype = 1;
            lapackf77_chegvd(&itype, "V", "L", &gramDim,
                             gevectors, &ldgram, h_gramB, &ldgram,
                             gevalues, hwork, &lwork,
                             #ifdef COMPLEX
                             rwork, &lrwork,
                             #endif
                             iwork, &liwork, &info);
 
            for(magma_int_t k =0; k<n; k++)
                evalues[k] = gevalues[k];
            
            // === copy back the result to gramA on the GPU and use it for the updates
            magma_csetmatrix( gramDim, gramDim, gevectors, ldgram, gramA, ldgram, queue );

            if (restart == 0) {
                // === contribution from P to the new X (in new search direction P)
                magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, n, cBlockSize,
                            c_one, blockP, m, gramA(n+cBlockSize,0), ldgram, c_zero, dwork, m, queue );
                SWAP(dwork, blockP);
 
                // === contribution from R to the new X (in new search direction P)
                magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, n, cBlockSize,
                            c_one, blockR, m, gramA(n,0), ldgram, c_one, blockP, m, queue );

                // === corresponding contribution from AP to the new AX (in AP)
                magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, n, cBlockSize,
                            c_one, blockAP, m, gramA(n+cBlockSize,0), ldgram, c_zero, dwork, m, queue );
                SWAP(dwork, blockAP);

                // === corresponding contribution from AR to the new AX (in AP)
                magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, n, cBlockSize,
                            c_one, blockAR, m, gramA(n,0), ldgram, c_one, blockAP, m, queue );
            }
            else {
                // === contribution from R (only) to the new X
                magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, n, cBlockSize,
                            c_one, blockR, m, gramA(n,0), ldgram, c_zero, blockP, m, queue );

                // === corresponding contribution from AR (only) to the new AX
                magma_cgemm( MagmaNoTrans, MagmaNoTrans,m, n, cBlockSize,
                            c_one, blockAR, m, gramA(n,0), ldgram, c_zero, blockAP, m, queue );
            }
            
            // === contribution from old X to the new X + the new search direction P
            magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, n, n,
                        c_one, blockX, m, gramA, ldgram, c_zero, dwork, m, queue );
            SWAP(dwork, blockX);
            //magma_caxpy( m*n, c_one, blockP, 1, blockX, 1, queue );
            CHECK( magma_clobpcg_maxpy( m, n, blockP, blockX, queue ));

            
            // === corresponding contribution from old AX to new AX + AP
            magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, n, n,
                        c_one, blockAX, m, gramA, ldgram, c_zero, dwork, m, queue );
            SWAP(dwork, blockAX);
            //magma_caxpy( m*n, c_one, blockAP, 1, blockAX, 1, queue );
            CHECK( magma_clobpcg_maxpy( m, n, blockAP, blockAX, queue ));

            condestGhistory[iterationNumber+1]=condestG;

            magma_sgetmatrix( 1, 1, residualNorms(0, iterationNumber), 1,  &tmp, 1, queue );
            if ( iterationNumber == 1 ) {
                solver_par->init_res = tmp;
                r0 = tmp * solver_par->rtol;
                if ( r0 < ATOLERANCE )
                    r0 = ATOLERANCE;
            }
            solver_par->final_res = tmp;
            if ( tmp < r0 ) {
                break;
            }
            if (cBlockSize == 0) {
                break;
            }

            if ( solver_par->verbose!=0 ) {
                if ( iterationNumber%solver_par->verbose == 0 ) {
                    // float res;
                    // magma_cgetmatrix( 1, 1,
                    //                  (magmaFloatComplex*)residualNorms(0, iterationNumber), 1,
                    //                  (magmaFloatComplex*)&res, 1, queue );
                    //
                    //  printf("Iteration %4d, CBS %4d, Residual: %10.7f\n",
                    //         iterationNumber, cBlockSize, res);
                    printf("%4d-%2d ", int(iterationNumber), int(cBlockSize));
                    magma_sprint_gpu( 1, n, residualNorms(0, iterationNumber), 1, queue );
                }
            }

            restart = 0;
        }   // === end for iterationNumber = 1,maxIterations =======================


    // fill solver info
    tempo2 = magma_sync_wtime( queue );
    solver_par->runtime = (real_Double_t) tempo2-tempo1;
    solver_par->numiter = iterationNumber;
    if ( solver_par->numiter < solver_par->maxiter) {
        info = MAGMA_SUCCESS;
    } else if ( solver_par->init_res > solver_par->final_res )
        info = MAGMA_SLOW_CONVERGENCE;
    else
        info = MAGMA_DIVERGENCE;
    
    // =============================================================================
    // === postprocessing;
    // =============================================================================

    // === compute the real AX and corresponding eigenvalues
    magma_c_bspmv_tuned(m, n, c_one, A, blockX, c_zero, blockAX, queue );
    magma_cgemm( MagmaConjTrans, MagmaNoTrans, n, n, m,
                c_one,  blockX, m, blockAX, m, c_zero, gramM, n, queue );

    magma_cheevd_gpu( MagmaVec, MagmaUpper,
                      n, gramM, n, gevalues, dwork, n, hwork, lwork,
                      #ifdef COMPLEX
                      rwork, lrwork,
                      #endif
                      iwork, liwork, &info );
   
    for(magma_int_t k =0; k<n; k++)
        evalues[k] = gevalues[k];

    // === update X = X * evectors
    SWAP(blockX, dwork);
    magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, n, n,
                c_one, dwork, m, gramM, n, c_zero, blockX, m, queue );

    // === update AX = AX * evectors to compute the final residual
    SWAP(blockAX, dwork);
    magma_cgemm( MagmaNoTrans, MagmaNoTrans, m, n, n,
                c_one, dwork, m, gramM, n, c_zero, blockAX, m, queue );

    // === compute R = AX - evalues X
    magmablas_clacpy( MagmaFull, m, n, blockAX, m, blockR, m, queue );
    for(magma_int_t i=0; i<n; i++)
        magma_caxpy( m, MAGMA_C_MAKE(-evalues[i], 0), blockX+i*m, 1, blockR+i*m, 1, queue );

    // === residualNorms[iterationNumber] = || R ||
    magmablas_scnrm2_cols( m, n, blockR, m, residualNorms(0, iterationNumber), queue );

    // === restore blockX if needed
    if (blockX != origX)
        magmablas_clacpy( MagmaFull, m, n, blockX, m, origX, m, queue );

    printf("Eigenvalues:\n");
    for(magma_int_t i =0; i<n; i++)
        printf("%e  ", evalues[i]);
    printf("\n\n");

    printf("Final residuals:\n");
    magma_sprint_gpu( 1, n, residualNorms(0, iterationNumber), 1, queue );
    printf("\n\n");

    //=== Prmagma_int_t residual history in a file for plotting ====
    CHECK( magma_smalloc_cpu(&hresidualNorms, (iterationNumber+1) * n));
    magma_sgetmatrix( n, iterationNumber,
                                        residualNorms, n,
                                        hresidualNorms, n, queue );
    solver_par->iter_res = *hresidualNorms(0, iterationNumber-1);

    printf("Residuals are stored in file residualNorms\n");
    printf("Plot the residuals using: myplot \n");
    
    FILE *residuals_file;
    residuals_file = fopen("residualNorms", "w");
    for(magma_int_t i =1; i<iterationNumber; i++) {
        for(magma_int_t j = 0; j<n; j++)
            fprintf(residuals_file, "%f ", *hresidualNorms(j,i));
        fprintf(residuals_file, "\n");
    }
    fclose(residuals_file);
    
cleanup:
    magma_free_cpu(hresidualNorms);

    // === free work space
    magma_free(     residualNorms   );
    magma_free_cpu( condestGhistory );
    magma_free_cpu( gevalues        );
    magma_free_cpu( iwork           );

    magma_free_pinned( hW           );
    magma_free_pinned( gevectors    );
    magma_free_pinned( h_gramB      );

    magma_free(     gramM           );
    magma_free(     gramA           );
    magma_free(     gramB           );
    magma_free(  activeMask         );

    if (blockX != (solver_par->eigenvectors))
        magma_free(     blockX    );
    if (blockAX != (solver_par->eigenvectors))
        magma_free(     blockAX    );
    if (blockAR != (solver_par->eigenvectors))
        magma_free(     blockAR    );
    if (blockAP != (solver_par->eigenvectors))
        magma_free(     blockAP    );
    if (blockR != (solver_par->eigenvectors))
        magma_free(     blockR    );
    if (blockP != (solver_par->eigenvectors))
        magma_free(     blockP    );
    if (blockW != (solver_par->eigenvectors))
        magma_free(     blockW    );
    if (dwork != (solver_par->eigenvectors))
        magma_free(     dwork    );
    magma_free(     eval_gpu    );

    magma_free_pinned( hwork    );


    #ifdef COMPLEX
    magma_free_cpu( rwork           );
    rwork = NULL;
    #endif

    return info; 
}
