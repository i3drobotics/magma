/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/control/magma_zpariluutils.cpp, normal z -> s, Sat Mar 27 20:32:40 2021
       @author Hartwig Anzt
*/
#include "magmasparse_internal.h"

#define PRECISION_s

/**
    Purpose
    -------

    Computes the Frobenius norm of the difference between the CSR matrices A
    and B. They need to share the same sparsity pattern!


    Arguments
    ---------

    @param[in]
    A           magma_s_matrix
                sparse matrix in CSR

    @param[in]
    B           magma_s_matrix
                sparse matrix in CSR
                
    @param[out]
    res         real_Double_t*
                Frobenius norm of difference
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_saux
    ********************************************************************/

magma_int_t
magma_sfrobenius(
    magma_s_matrix A,
    magma_s_matrix B,
    real_Double_t *res,
    magma_queue_t queue )
{
    real_Double_t tmp2;
    magma_int_t i,j,k;
    *res = 0.0;
    
    for(i=0; i<A.num_rows; i++){
        for(j=A.row[i]; j<A.row[i+1]; j++){
            magma_index_t localcol = A.col[j];
            for( k=B.row[i]; k<B.row[i+1]; k++){
                if(B.col[k] == localcol){
                    tmp2 = (real_Double_t) fabs( MAGMA_S_REAL(A.val[j] )
                                                    - MAGMA_S_REAL(B.val[k]) );

                    (*res) = (*res) + tmp2* tmp2;
                }
            }
        }
    }

    (*res) =  sqrt((*res));

    return MAGMA_SUCCESS;
}



/**
    Purpose
    -------

    Computes the nonlinear residual A - LU and returns the difference as
    well es the Frobenius norm of the difference


    Arguments
    ---------

    @param[in]
    A           magma_s_matrix
                input sparse matrix in CSR

    @param[in]
    L           magma_s_matrix
                input sparse matrix in CSR

    @param[in]
    U           magma_s_matrix
                input sparse matrix in CSR

    @param[out]
    LU          magma_s_matrix*
                output sparse matrix in A-LU in CSR

    @param[out]
    res         real_Double_t*
                Frobenius norm of difference
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_saux
    ********************************************************************/

magma_int_t
magma_snonlinres(
    magma_s_matrix A,
    magma_s_matrix L,
    magma_s_matrix U,
    magma_s_matrix *LU,
    real_Double_t *res,
    magma_queue_t queue )
{
    magma_int_t info = 0;

    real_Double_t tmp2;
    magma_int_t i,j,k;
        
    float one = MAGMA_S_MAKE( 1.0, 0.0 );

    magma_s_matrix dL={Magma_CSR}, dU={Magma_CSR}, dLU={Magma_CSR}, A_t={Magma_CSR};
    
    // make sure the target structure is empty
    magma_smfree( LU, queue );

    CHECK( magma_smtransfer( L, &dL, Magma_CPU, Magma_DEV, queue  ));
    CHECK( magma_smtransfer( U, &dU, Magma_CPU, Magma_DEV, queue  ));
    CHECK( magma_smtransfer( A, &A_t, Magma_CPU, Magma_CPU, queue  ));
    CHECK( magma_s_spmm( one, dL, dU, &dLU, queue ));

    CHECK( magma_smtransfer(dLU, LU, Magma_DEV, Magma_CPU, queue ));
    magma_smfree( &dL, queue  );
    magma_smfree( &dU, queue  );
    magma_smfree( &dLU, queue  );

    // compute Frobenius norm of A-LU
    for(i=0; i<A.num_rows; i++){
        for(j=A.row[i]; j<A.row[i+1]; j++){
            magma_index_t lcol = A.col[j];
            float newval = MAGMA_S_MAKE(0.0, 0.0);
            for(k=LU->row[i]; k<LU->row[i+1]; k++){
                if( LU->col[k] == lcol ){
                    newval = MAGMA_S_MAKE(
                        MAGMA_S_REAL( LU->val[k] )- MAGMA_S_REAL( A.val[j] )
                                                , 0.0 );
                }
            }
            A_t.val[j] = newval;
        }
    }

    for(i=0; i<A.num_rows; i++){
        for(j=A.row[i]; j<A.row[i+1]; j++){
            tmp2 = (real_Double_t) fabs( MAGMA_S_REAL(A_t.val[j]) );
            (*res) = (*res) + tmp2* tmp2;
        }
    }

    magma_smfree( LU, queue  );
    magma_smfree( &A_t, queue  );

    (*res) =  sqrt((*res));
    
cleanup:
    if( info !=0 ){
        magma_smfree( LU, queue  );
    }
    magma_smfree( &A_t, queue  );
    magma_smfree( &dL, queue  );
    magma_smfree( &dU, queue  );
    magma_smfree( &dLU, queue  );
    return info;
}

/**
    Purpose
    -------

    Computes the ILU residual A - LU and returns the difference as
    well es the Frobenius norm of the difference


    Arguments
    ---------

    @param[in]
    A           magma_s_matrix
                input sparse matrix in CSR

    @param[in]
    L           magma_s_matrix
                input sparse matrix in CSR

    @param[in]
    U           magma_s_matrix
                input sparse matrix in CSR

    @param[out]
    LU          magma_s_matrix*
                output sparse matrix in A-LU in CSR
                
    @param[out]
    res         real_Double_t*
                Frobenius norm of difference
                
    @param[out]
    nonlinres   real_Double_t*
                Frobenius norm of difference
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_saux
    ********************************************************************/

magma_int_t
magma_silures(
    magma_s_matrix A,
    magma_s_matrix L,
    magma_s_matrix U,
    magma_s_matrix *LU,
    real_Double_t *res,
    real_Double_t *nonlinres,
    magma_queue_t queue )
{
    magma_int_t info = 0;

    float tmp;
    real_Double_t tmp2;
    magma_int_t i, j, k;
    
    float one = MAGMA_S_MAKE( 1.0, 0.0 );

    magma_s_matrix LL={Magma_CSR}, dL={Magma_CSR}, dU={Magma_CSR}, dLU={Magma_CSR};
    
    // make sure the target structure is empty
    magma_smfree( LU, queue );

    if( L.row[1]==1 ){        // lower triangular with unit diagonal
        //printf("L lower triangular.\n");
        LL.diagorder_type = Magma_UNITY;
        CHECK( magma_smconvert( L, &LL, Magma_CSR, Magma_CSRL, queue ));
    }
    else if ( L.row[1]==0 ){ // strictly lower triangular
        //printf("L strictly lower triangular.\n");
        CHECK( magma_smtransfer( L, &LL, Magma_CPU, Magma_CPU, queue ));
        magma_free_cpu( LL.col );
        magma_free_cpu( LL.val );
        LL.nnz = L.nnz+L.num_rows;
        CHECK( magma_smalloc_cpu( &LL.val, LL.nnz ));
        CHECK( magma_index_malloc_cpu( &LL.col, LL.nnz ));
        magma_int_t z=0;
        for (i=0; i < L.num_rows; i++) {
            LL.row[i] = z;
            for (j=L.row[i]; j < L.row[i+1]; j++) {
                LL.val[z] = L.val[j];
                LL.col[z] = L.col[j];
                z++;
            }
            // add unit diagonal
            LL.val[z] = MAGMA_S_MAKE(1.0, 0.0);
            LL.col[z] = i;
            z++;
        }
        LL.row[LL.num_rows] = z;
    }
    else {
        printf("error: L neither lower nor strictly lower triangular!\n");
    }

    CHECK( magma_smtransfer( LL, &dL, Magma_CPU, Magma_DEV, queue  ));
    CHECK( magma_smtransfer( U, &dU, Magma_CPU, Magma_DEV, queue  ));
    magma_smfree( &LL, queue );
    CHECK( magma_s_spmm( one, dL, dU, &dLU, queue ));



    CHECK( magma_smtransfer(dLU, LU, Magma_DEV, Magma_CPU, queue ));
    magma_smfree( &dL, queue );
    magma_smfree( &dU, queue );
    magma_smfree( &dLU, queue );

    // compute Frobenius norm of A-LU
    for(i=0; i<A.num_rows; i++){
        for(j=A.row[i]; j<A.row[i+1]; j++){
            magma_index_t lcol = A.col[j];
            for(k=LU->row[i]; k<LU->row[i+1]; k++){
                if( LU->col[k] == lcol ){
                    tmp = MAGMA_S_MAKE(
                        MAGMA_S_REAL( LU->val[k] )- MAGMA_S_REAL( A.val[j] )
                                                , 0.0 );
                    LU->val[k] = tmp;

                    tmp2 = (real_Double_t) fabs( MAGMA_S_REAL(tmp) );
                    (*nonlinres) = (*nonlinres) + tmp2*tmp2;
                }
            }
        }
    }

    for(i=0; i<LU->num_rows; i++){
        for(j=LU->row[i]; j<LU->row[i+1]; j++){
            tmp2 = (real_Double_t) fabs( MAGMA_S_REAL(LU->val[j]) );
            (*res) = (*res) + tmp2* tmp2;
        }
    }

    (*res) =  sqrt((*res));
    (*nonlinres) =  sqrt((*nonlinres));

cleanup:
    if( info !=0 ){
        magma_smfree( LU, queue  );
    }
    magma_smfree( &LL, queue );
    magma_smfree( &dL, queue  );
    magma_smfree( &dU, queue  );
    magma_smfree( &dLU, queue  );
    return info;
}



/**
    Purpose
    -------

    Computes the IC residual A - CC^T and returns the difference as
    well es the Frobenius norm of the difference


    Arguments
    ---------

    @param[in]
    A           magma_s_matrix
                input sparse matrix in CSR

    @param[in]
    C           magma_s_matrix
                input sparse matrix in CSR

    @param[in]
    CT          magma_s_matrix
                input sparse matrix in CSR

    @param[in]
    LU          magma_s_matrix*
                output sparse matrix in A-LU in CSR

    @param[out]
    res         real_Double_t*
                IC residual
                
    @param[out]
    nonlinres   real_Double_t*
                nonlinear residual
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_saux
    ********************************************************************/

magma_int_t
magma_sicres(
    magma_s_matrix A,
    magma_s_matrix C,
    magma_s_matrix CT,
    magma_s_matrix *LU,
    real_Double_t *res,
    real_Double_t *nonlinres,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    
    float tmp;
    real_Double_t tmp2;
    magma_int_t i,j,k;

    float one = MAGMA_S_MAKE( 1.0, 0.0 );
    
    magma_s_matrix dL={Magma_CSR}, dU={Magma_CSR}, dLU={Magma_CSR};
    
    // make sure the target structure is empty
    magma_smfree( LU, queue );
    
    *res = 0.0;
    *nonlinres = 0.0;

    CHECK( magma_smtransfer( C, &dL, Magma_CPU, Magma_DEV, queue ));
    CHECK( magma_smtransfer( CT, &dU, Magma_CPU, Magma_DEV, queue ));
    CHECK( magma_s_spmm( one, dL, dU, &dLU, queue ));
    CHECK( magma_smtransfer(dLU, LU, Magma_DEV, Magma_CPU, queue ));

    magma_smfree( &dLU, queue );

    // compute Frobenius norm of A-LU
    for(i=0; i<A.num_rows; i++){
        for(j=A.row[i]; j<A.row[i+1]; j++){
            magma_index_t lcol = A.col[j];
            for(k=LU->row[i]; k<LU->row[i+1]; k++){
                if( LU->col[k] == lcol ){
                    tmp = MAGMA_S_MAKE(
                        MAGMA_S_REAL( LU->val[k] )- MAGMA_S_REAL( A.val[j] )
                                                , 0.0 );
                    LU->val[k] = tmp;

                    tmp2 = (real_Double_t) fabs( MAGMA_S_REAL(tmp) );
                    (*nonlinres) = (*nonlinres) + tmp2*tmp2;
                }
            }
        }
    }

    for(i=0; i<LU->num_rows; i++){
        for(j=LU->row[i]; j<LU->row[i+1]; j++){
            tmp2 = (real_Double_t) fabs( MAGMA_S_REAL(LU->val[j]) );
            (*res) = (*res) + tmp2* tmp2;
        }
    }


    (*res) =  sqrt((*res));
    (*nonlinres) =  sqrt((*nonlinres));

cleanup:
    if( info !=0 ){
        magma_smfree( LU, queue  );
    }
    magma_smfree( &dL, queue  );
    magma_smfree( &dU, queue  );
    magma_smfree( &dLU, queue  );
    return info;
}



/**
    Purpose
    -------

    Computes an initial guess for the ParILU/ParIC


    Arguments
    ---------

    @param[in]
    A           magma_s_matrix
                sparse matrix in CSR

    @param[out]
    L           magma_s_matrix*
                sparse matrix in CSR

    @param[out]
    U           magma_s_matrix*
                sparse matrix in CSR
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.


    @ingroup magmasparse_saux
    ********************************************************************/

magma_int_t
magma_sinitguess(
    magma_s_matrix A,
    magma_s_matrix *L,
    magma_s_matrix *U,
    magma_queue_t queue )
{
    magma_int_t info = 0;

    float one = MAGMA_S_MAKE( 1.0, 0.0 );
    
    magma_s_matrix hAL={Magma_CSR}, hAU={Magma_CSR}, dAL={Magma_CSR}, 
    dAU={Magma_CSR}, dALU={Magma_CSR}, hALU={Magma_CSR}, hD={Magma_CSR}, 
    dD={Magma_CSR}, dL={Magma_CSR}, hL={Magma_CSR};
    magma_int_t i,j;
    
    magma_int_t offdiags = 0;
    magma_index_t *diag_offset;
    float *diag_vals=NULL;
    
    // make sure the target structure is empty
    magma_smfree( L, queue );
    magma_smfree( U, queue );

    // need only lower triangular
    hAL.diagorder_type = Magma_VALUE;
    CHECK( magma_smconvert( A, &hAL, Magma_CSR, Magma_CSRL, queue ));
    //magma_smconvert( hAL, &hALCOO, Magma_CSR, Magma_CSRCOO );

    // need only upper triangular
    //magma_smconvert( A, &hAU, Magma_CSR, Magma_CSRU );
    CHECK( magma_s_cucsrtranspose(  hAL, &hAU, queue ));
    //magma_smconvert( hAU, &hAUCOO, Magma_CSR, Magma_CSRCOO );
    CHECK( magma_smtransfer( hAL, &dAL, Magma_CPU, Magma_DEV, queue ));
    CHECK( magma_s_spmm( one, dAL, dAU, &dALU, queue ));
    CHECK( magma_smtransfer( dALU, &hALU, Magma_DEV, Magma_CPU, queue ));

    magma_smfree( &dAU, queue);
    magma_smfree( &dALU, queue);


    CHECK( magma_smalloc_cpu( &diag_vals, offdiags+1 ));
    CHECK( magma_index_malloc_cpu( &diag_offset, offdiags+1 ));
    diag_offset[0] = 0;
    diag_vals[0] = MAGMA_S_MAKE( 1.0, 0.0 );
    CHECK( magma_smgenerator( hALU.num_rows, offdiags, diag_offset, diag_vals, &hD, queue ));
    magma_smfree( &hALU, queue );

    
    for(i=0; i<hALU.num_rows; i++){
        for(j=hALU.row[i]; j<hALU.row[i+1]; j++){
            if( hALU.col[j] == i ){
                //printf("%d %d  %d == %d -> %f   -->", i, j, hALU.col[j], i, hALU.val[j]);
                hD.val[i] = MAGMA_S_MAKE(
                        1.0 / sqrt(fabs(MAGMA_S_REAL(hALU.val[j])))  , 0.0 );
                //printf("insert %f at %d\n", hD.val[i], i);
            }
        }
    }


    CHECK( magma_smtransfer( hD, &dD, Magma_CPU, Magma_DEV, queue ));
    magma_smfree( &hD, queue);

    CHECK( magma_s_spmm( one, dD, dAL, &dL, queue ));
    magma_smfree( &dAL, queue );
    magma_smfree( &dD, queue );



/*
    // check for diagonal = 1
    magma_s_matrix dLt={Magma_CSR}, dLL={Magma_CSR}, LL={Magma_CSR};
    CHECK( magma_s_cucsrtranspose(  dL, &dLt ));
    CHECK( magma_scuspmm( dL, dLt, &dLL ));
    CHECK( magma_smtransfer( dLL, &LL, Magma_DEV, Magma_CPU ));
    //for(i=0; i < hALU.num_rows; i++) {
    for(i=0; i < 100; i++) {
        for(j=hALU.row[i]; j < hALU.row[i+1]; j++) {
            if( hALU.col[j] == i ){
                printf("%d %d -> %f   -->", i, i, LL.val[j]);
            }
        }
    }
*/
    CHECK( magma_smtransfer( dL, &hL, Magma_DEV, Magma_CPU, queue ));
    CHECK( magma_smconvert( hL, L, Magma_CSR, Magma_CSRCOO, queue ));



cleanup:
    if( info !=0 ){
        magma_smfree( L, queue  );
        magma_smfree( U, queue  );
    }
    magma_smfree( &dAU, queue);
    magma_smfree( &dALU, queue);
    magma_smfree( &dL, queue );
    magma_smfree( &hL, queue );
    magma_smfree( &dAL, queue );
    magma_smfree( &dD, queue );
    magma_smfree( &hD, queue);
    magma_smfree( &hALU, queue );
    return info;
}



/**
    Purpose
    -------

    Using the iterative approach of computing ILU factorizations with increasing
    fill-in, it takes the input matrix A, containing the approximate factors,
    ( L and U as well )
    computes a matrix with one higher level of fill-in, inserts the original
    approximation as initial guess, and provides the factors L and U also
    filled with the scaled initial guess.


    Arguments
    ---------

    @param[in]
    A           magma_s_matrix*
                sparse matrix in CSR

    @param[out]
    B           magma_s_matrix*
                sparse matrix in CSR
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.


    @ingroup magmasparse_saux
    ********************************************************************/

magma_int_t
magma_sinitrecursiveLU(
    magma_s_matrix A,
    magma_s_matrix *B,
    magma_queue_t queue )
{
    magma_int_t i,j,k;
    
    // make sure the target structure is empty
    magma_smfree( B, queue );

    for(i=0; i<A.num_rows; i++){
        for(j=B->row[i]; j<B->row[i+1]; j++){
            B->val[j] = MAGMA_S_MAKE(0.0, 0.0);
            magma_index_t localcol = B->col[j];
            for( k=A.row[i]; k<A.row[i+1]; k++){
                if(A.col[k] == localcol){
                    B->val[j] = A.val[k];
                }
            }
        }
    }

    return MAGMA_SUCCESS; 
}



/**
    Purpose
    -------

    Checks for a lower triangular matrix whether it is strictly lower triangular
    and in the negative case adds a unit diagonal. It does this in-place.


    Arguments
    ---------

    @param[in,out]
    L           magma_s_matrix*
                sparse matrix in CSR
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_saux
    ********************************************************************/

magma_int_t
magma_smLdiagadd(
    magma_s_matrix *L,
    magma_queue_t queue )
{
    magma_int_t info = 0;

    magma_s_matrix LL={Magma_CSR};

    if( L->row[1]==1 ){        // lower triangular with unit diagonal
        //printf("L lower triangular.\n");
        LL.diagorder_type = Magma_UNITY;
        CHECK( magma_smconvert( *L, &LL, Magma_CSR, Magma_CSRL, queue ));
    }
    else if ( L->row[1]==0 ){ // strictly lower triangular
        //printf("L strictly lower triangular.\n");
        CHECK( magma_smtransfer( *L, &LL, Magma_CPU, Magma_CPU, queue ));
        magma_free_cpu( LL.col );
        magma_free_cpu( LL.val );
        LL.nnz = L->nnz+L->num_rows;
        CHECK( magma_smalloc_cpu( &LL.val, LL.nnz ));
        CHECK( magma_index_malloc_cpu( &LL.col, LL.nnz ));
        magma_int_t z=0;
        for( magma_int_t i=0; i<L->num_rows; i++){
            LL.row[i] = z;
            for( magma_int_t j=L->row[i]; j<L->row[i+1]; j++){
                LL.val[z] = L->val[j];
                LL.col[z] = L->col[j];
                z++;
            }
            // add unit diagonal
            LL.val[z] = MAGMA_S_MAKE(1.0, 0.0);
            LL.col[z] = i;
            z++;
        }
        LL.row[LL.num_rows] = z;
        LL.nnz = z;
    }
    else {
        printf("error: L neither lower nor strictly lower triangular!\n");
    }
    magma_smfree( L, queue );
    CHECK( magma_smtransfer(LL, L, Magma_CPU, Magma_CPU, queue ));

cleanup:
    if( info != 0 ){
        magma_smfree( L, queue );
    }
    magma_smfree( &LL, queue );
    return info;
}
