/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/control/magma_zvpass_gpu.cpp, normal z -> d, Sat Mar 27 20:32:46 2021
       @author Hartwig Anzt
*/

#include "magmasparse_internal.h"


/**
    Purpose
    -------

    Passes a vector to MAGMA (located on DEV).

    Arguments
    ---------

    @param[in]
    m           magma_int_t
                number of rows

    @param[in]
    n           magma_int_t
                number of columns

    @param[in]
    val         double*
                array containing vector entries

    @param[out]
    v           magma_d_matrix*
                magma vector
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_daux
    ********************************************************************/

extern "C"
magma_int_t
magma_dvset_dev(
    magma_int_t m, magma_int_t n,
    magmaDouble_ptr val,
    magma_d_matrix *v,
    magma_queue_t queue )
{
    
    // make sure the target structure is empty
    magma_dmfree( v, queue );
    
    v->num_rows = m;
    v->num_cols = n;
    v->nnz = m*n;
    v->memory_location = Magma_DEV;
    v->storage_type = Magma_DENSE;
    v->dval = val;
    v->major = MagmaColMajor;
    v->ownership = MagmaFalse;
    
    return MAGMA_SUCCESS;
}



/**
    Purpose
    -------

    Passes a MAGMA vector back.

    Arguments
    ---------

    @param[in]
    v           magma_d_matrix
                magma vector

    @param[out]
    m           magma_int_t
                number of rows

    @param[out]
    n           magma_int_t
                number of columns

    @param[out]
    val         double*
                array containing vector entries

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_daux
    ********************************************************************/

extern "C"
magma_int_t
magma_dvget(
    magma_d_matrix v,
    magma_int_t *m, magma_int_t *n,
    double **val,
    magma_queue_t queue )
{
    magma_d_matrix v_CPU={Magma_CSR};
    magma_int_t info =0;
    
    if ( v.memory_location == Magma_CPU ) {
        *m = v.num_rows;
        *n = v.num_cols;
        *val = v.val;
    } else {
        CHECK( magma_dmtransfer( v, &v_CPU, v.memory_location, Magma_CPU, queue ));
        CHECK( magma_dvget( v_CPU, m, n, val, queue ));
    }
    
cleanup:
    return info;
}


/**
    Purpose
    -------

    Passes a MAGMA vector back (located on DEV).

    Arguments
    ---------

    @param[in]
    v           magma_d_matrix
                magma vector

    @param[out]
    m           magma_int_t
                number of rows

    @param[out]
    n           magma_int_t
                number of columns

    @param[out]
    val         magmaDouble_ptr
                array containing vector entries

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_daux
    ********************************************************************/

extern "C"
magma_int_t
magma_dvget_dev(
    magma_d_matrix v,
    magma_int_t *m, magma_int_t *n,
    magmaDouble_ptr *val,
    magma_queue_t queue )
{
    magma_int_t info =0;
    
    magma_d_matrix v_DEV={Magma_CSR};
    
    if ( v.memory_location == Magma_DEV ) {
        *m = v.num_rows;
        *n = v.num_cols;
        *val = v.dval;
    } else {
        CHECK( magma_dmtransfer( v, &v_DEV, v.memory_location, Magma_DEV, queue ));
        CHECK( magma_dvget_dev( v_DEV, m, n, val, queue ));
    }
    
cleanup:
    magma_dmfree( &v_DEV, queue );
    return info;
}


/**
    Purpose
    -------

    Passes a MAGMA vector back (located on DEV).
    This function requires the array val to be 
    already allocated (of size m x n).

    Arguments
    ---------

    @param[in]
    v           magma_d_matrix
                magma vector

    @param[out]
    m           magma_int_t
                number of rows

    @param[out]
    n           magma_int_t
                number of columns

    @param[out]
    val         double*
                array of size m x n on the device the vector entries 
                are copied into
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_daux
    ********************************************************************/

extern "C"
magma_int_t
magma_dvcopy_dev(
    magma_d_matrix v,
    magma_int_t *m, magma_int_t *n,
    magmaDouble_ptr val,
    magma_queue_t queue )
{
    magma_int_t info =0;
    
    magma_d_matrix v_DEV={Magma_CSR};
    
    if ( v.memory_location == Magma_DEV ) {
        *m = v.num_rows;
        *n = v.num_cols;
        magma_dcopyvector( v.num_rows * v.num_cols, v.dval, 1, val, 1, queue );
    } else {
        CHECK( magma_dmtransfer( v, &v_DEV, v.memory_location, Magma_DEV, queue ));
        CHECK( magma_dvcopy_dev( v_DEV, m, n, val, queue ));
    }
    
cleanup:
    magma_dmfree( &v_DEV, queue );
    return info;
}
