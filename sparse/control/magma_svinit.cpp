/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/control/magma_zvinit.cpp, normal z -> s, Sat Mar 27 20:32:45 2021
       @author Hartwig Anzt
*/
#include "magmasparse_internal.h"
//#include <stdlib.h>

/**
    Purpose
    -------

    Allocates memory for magma_s_matrix and initializes it
    with the passed value.


    Arguments
    ---------

    @param[out]
    x           magma_s_matrix*
                vector to initialize

    @param[in]
    mem_loc     magma_location_t
                memory for vector

    @param[in]
    num_rows    magma_int_t
                desired length of vector
                
    @param[in]
    num_cols    magma_int_t
                desired width of vector-block (columns of dense matrix)

    @param[in]
    values      float
                entries in vector

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_saux
    ********************************************************************/

extern "C" magma_int_t
magma_svinit(
    magma_s_matrix *x,
    magma_location_t mem_loc,
    magma_int_t num_rows,
    magma_int_t num_cols,
    float values,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    
    // make sure the target structure is empty
    magma_smfree( x, queue );
    x->ownership = MagmaTrue;
    x->val = NULL;
    x->diag = NULL;
    x->row = NULL;
    x->rowidx = NULL;
    x->col = NULL;
    x->list = NULL;
    x->blockinfo = NULL;
    x->dval = NULL;
    x->ddiag = NULL;
    x->drow = NULL;
    x->drowidx = NULL;
    x->dcol = NULL;
    x->dlist = NULL;
    x->storage_type = Magma_DENSE;
    x->memory_location = mem_loc;
    x->sym = Magma_GENERAL;
    x->diagorder_type = Magma_VALUE;
    x->fill_mode = MagmaFull;
    x->num_rows = num_rows;
    x->num_cols = num_cols;
    x->nnz = num_rows*num_cols;
    x->max_nnz_row = num_cols;
    x->diameter = 0;
    x->blocksize = 1;
    x->numblocks = 1;
    x->alignment = 1;
    x->major = MagmaColMajor;
    x->ld = num_rows;
    if ( mem_loc == Magma_CPU ) {
        CHECK( magma_smalloc_cpu( &x->val, x->nnz ));
        for( magma_int_t i=0; i<x->nnz; i++) {
             x->val[i] = values;
        }
    }
    else if ( mem_loc == Magma_DEV ) {
        CHECK( magma_smalloc( &x->val, x->nnz ));
        magmablas_slaset( MagmaFull, x->num_rows, x->num_cols, values, values, x->val, x->num_rows, queue );
    }
    
cleanup:
    return info; 
}



/**
    Purpose
    -------

    Allocates memory for magma_s_matrix and initializes it
    with random values.


    Arguments
    ---------

    @param[out]
    x           magma_s_matrix*
                vector to initialize

    @param[in]
    mem_loc     magma_location_t
                memory for vector

    @param[in]
    num_rows    magma_int_t
                desired length of vector
                
    @param[in]
    num_cols    magma_int_t
                desired width of vector-block (columns of dense matrix)

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_saux
    ********************************************************************/

extern "C" magma_int_t
magma_svinit_rand(
    magma_s_matrix *x,
    magma_location_t mem_loc,
    magma_int_t num_rows,
    magma_int_t num_cols,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    
    // make sure the target structure is empty
    magma_smfree( x, queue );
    x->ownership = MagmaTrue;
    magma_s_matrix x_h = {Magma_CSR};
    
    x->val = NULL;
    x->diag = NULL;
    x->row = NULL;
    x->rowidx = NULL;
    x->col = NULL;
    x->list = NULL;
    x->blockinfo = NULL;
    x->dval = NULL;
    x->ddiag = NULL;
    x->drow = NULL;
    x->drowidx = NULL;
    x->dcol = NULL;
    x->dlist = NULL;
    x->storage_type = Magma_DENSE;
    x->memory_location = mem_loc;
    x->sym = Magma_GENERAL;
    x->diagorder_type = Magma_VALUE;
    x->fill_mode = MagmaFull;
    x->num_rows = num_rows;
    x->num_cols = num_cols;
    x->nnz = num_rows*num_cols;
    x->max_nnz_row = num_cols;
    x->diameter = 0;
    x->blocksize = 1;
    x->numblocks = 1;
    x->alignment = 1;
    x->major = MagmaColMajor;
    x->ld = num_rows;
    if ( mem_loc == Magma_CPU ) {
         /* Intializes random number generator */
         srand((unsigned) 1);
        
        CHECK( magma_smalloc_cpu( &x->val, x->nnz ));
        for( magma_int_t i=0; i<x->nnz; i++) {
             x->val[i] = MAGMA_S_MAKE( ((float)(2*rand()))/ RAND_MAX - 1., ((float)(2*rand()))/ RAND_MAX - 1. );
        }
    }
    else if ( mem_loc == Magma_DEV ) {
        CHECK( magma_svinit_rand( &x_h, Magma_CPU, num_rows, num_cols, queue ));
        CHECK( magma_smtransfer( x_h, x, Magma_CPU, Magma_DEV, queue ) );
    }
    
cleanup:
    magma_smfree( &x_h, queue );
    return info; 
}
