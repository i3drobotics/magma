/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/blas/magma_zcuspmm.cpp, normal z -> d, Sat Mar 27 20:32:31 2021
       @author Hartwig Anzt

*/
#include "magmasparse_internal.h"

#define RTOLERANCE     lapackf77_dlamch( "E" )
#define ATOLERANCE     lapackf77_dlamch( "E" )

#if CUDA_VERSION >= 11000
#define cusparseXcsrgemmNnz(handle, transA, transB, m, n, k, descrA, nnzA, drowA, dcolA,        \
                            descrB, nnzB, drowB, dcolB, descrC, drowC, nnzTotal )               \
    {                                                                                           \
        cusparseMatDescr_t descrD;                                                              \
        int nnzD, *drowD, *dcolD;                                                               \
        csrgemm2Info_t linfo = NULL;                                                            \
        size_t bufsize;                                                                         \
        void *buf;                                                                              \
        double alpha = MAGMA_D_ONE, *beta = NULL;                                      \
        cusparseSetPointerMode(handle, CUSPARSE_POINTER_MODE_HOST);                             \
        cusparseCreateCsrgemm2Info(&linfo);                                                     \
        cusparseDcsrgemm2_bufferSizeExt(handle, m, n, k, &alpha,                                \
                                        descrA, nnzA, drowA, dcolA,                             \
                                        descrB, nnzB, drowB, dcolB,                             \
                                        beta,                                                   \
                                        descrD, nnzD, drowD, dcolD,                             \
                                        linfo, &bufsize);                                       \
        if (bufsize > 0)                                                                        \
           magma_malloc(&buf, bufsize);                                                         \
        cusparseXcsrgemm2Nnz(handle, m, n, k,                                                   \
                             descrA, nnzA, drowA, dcolA,                                        \
                             descrB, nnzB, drowB, dcolB,                                        \
                             descrD, nnzD, drowD, dcolD,                                        \
                             descrC, drowC, nnzTotal,                                           \
                             linfo, buf);                                                       \
        if (bufsize > 0)                                                                        \
           magma_free(buf);                                                                     \
    }
#endif

// todo: info and buf are passed from the above function
// also at the end destroy info: cusparseDestroyCsrgemm2Info(info);
#if CUDA_VERSION >= 11000
#define cusparseDcsrgemm(handle, transA, transB, m, n, k, descrA, nnzA, dvalA, drowA, dcolA,    \
                         descrB, nnzB, dvalB, drowB, dcolB, descrC, dvalC, drowC, dcolC )       \
    {                                                                                           \
        cusparseMatDescr_t descrD;                                                              \
        int nnzD, *drowD, *dcolD;                                                               \
        double *dvalD, alpha = MAGMA_D_ONE, *beta = NULL;                              \
        void *buf;                                                                              \
        csrgemm2Info_t linfo = NULL;                                                            \
        printf("cusparseDcsrgemm bufsize = %d\n", -1);                                          \
        cusparseDcsrgemm2(handle, m, n, k, &alpha,                                              \
                          descrA, nnzA, dvalA, drowA, dcolA,                                    \
                          descrB, nnzB, dvalB, drowB, dcolB,                                    \
                          beta,                                                                 \
                          descrD, nnzD, dvalD, drowD, dcolD,                                    \
                          descrC, dvalC, drowC, dcolC,                                          \
                          linfo, buf);                                                          \
    }
#endif


/**
    Purpose
    -------

    This is an interface to the cuSPARSE routine csrmm computing the product
    of two sparse matrices stored in csr format.


    Arguments
    ---------

    @param[in]
    A           magma_d_matrix
                input matrix

    @param[in]
    B           magma_d_matrix
                input matrix

    @param[out]
    AB          magma_d_matrix*
                output matrix AB = A * B

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_dblas
    ********************************************************************/

extern "C" magma_int_t
magma_dcuspmm(
    magma_d_matrix A, magma_d_matrix B,
    magma_d_matrix *AB,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    
    
    magma_d_matrix C={Magma_CSR};
    C.num_rows = A.num_rows;
    C.num_cols = B.num_cols;
    C.storage_type = A.storage_type;
    C.memory_location = A.memory_location;
    C.fill_mode = MagmaFull;
    
    C.val = NULL;
    C.col = NULL;
    C.row = NULL;
    C.rowidx = NULL;
    C.blockinfo = NULL;
    C.diag = NULL;
    C.dval = NULL;
    C.dcol = NULL;
    C.drow = NULL;
    C.drowidx = NULL;
    C.ddiag = NULL;
    
    magma_index_t base_t, nnz_t, baseC;
    
    cusparseHandle_t handle=NULL;
    cusparseMatDescr_t descrA=NULL;
    cusparseMatDescr_t descrB=NULL;
    cusparseMatDescr_t descrC=NULL;
    
    if (    A.memory_location == Magma_DEV
        && B.memory_location == Magma_DEV
        && ( A.storage_type == Magma_CSR ||
             A.storage_type == Magma_CSRCOO )
        && ( B.storage_type == Magma_CSR ||
             B.storage_type == Magma_CSRCOO ) )
    {
        // CUSPARSE context /
        CHECK_CUSPARSE( cusparseCreate( &handle ));
        CHECK_CUSPARSE( cusparseSetStream( handle, queue->cuda_stream() ));
        CHECK_CUSPARSE( cusparseCreateMatDescr( &descrA ));
        CHECK_CUSPARSE( cusparseCreateMatDescr( &descrB ));
        CHECK_CUSPARSE( cusparseCreateMatDescr( &descrC ));
        CHECK_CUSPARSE( cusparseSetMatType( descrA, CUSPARSE_MATRIX_TYPE_GENERAL ));
        CHECK_CUSPARSE( cusparseSetMatType( descrB, CUSPARSE_MATRIX_TYPE_GENERAL ));
        CHECK_CUSPARSE( cusparseSetMatType( descrC, CUSPARSE_MATRIX_TYPE_GENERAL ));
        CHECK_CUSPARSE( cusparseSetMatIndexBase( descrA, CUSPARSE_INDEX_BASE_ZERO ));
        CHECK_CUSPARSE( cusparseSetMatIndexBase( descrB, CUSPARSE_INDEX_BASE_ZERO ));
        CHECK_CUSPARSE( cusparseSetMatIndexBase( descrC, CUSPARSE_INDEX_BASE_ZERO ));

        // nnzTotalDevHostPtr points to host memory
        magma_index_t *nnzTotalDevHostPtr = (magma_index_t*) &C.nnz;
        CHECK_CUSPARSE( cusparseSetPointerMode( handle, CUSPARSE_POINTER_MODE_HOST ));
        CHECK( magma_index_malloc( &C.drow, (A.num_rows + 1) ));
        cusparseXcsrgemmNnz( handle, CUSPARSE_OPERATION_NON_TRANSPOSE,
                             CUSPARSE_OPERATION_NON_TRANSPOSE,
                             A.num_rows, B.num_cols, A.num_cols,
                             descrA, A.nnz, A.drow, A.dcol,
                             descrB, B.nnz, B.drow, B.dcol,
                             descrC, C.drow, nnzTotalDevHostPtr );
        if (NULL != nnzTotalDevHostPtr) {
            C.nnz = *nnzTotalDevHostPtr;
        } else {
            // workaround as nnz and base C are magma_int_t
            magma_index_getvector( 1, C.drow+C.num_rows, 1, &nnz_t, 1, queue );
            magma_index_getvector( 1, C.drow,   1, &base_t,    1, queue );
            C.nnz = (magma_int_t) nnz_t;
            baseC = (magma_int_t) base_t;
            C.nnz -= baseC;
        }
        CHECK( magma_index_malloc( &C.dcol, C.nnz ));
        CHECK( magma_dmalloc( &C.dval, C.nnz ));
        #ifdef HAVE_HIP
        hipsparseDcsrgemm( handle, HIPSPARSE_OPERATION_NON_TRANSPOSE,
                          HIPSPARSE_OPERATION_NON_TRANSPOSE,
                          A.num_rows, B.num_cols, A.num_cols,
                          descrA, A.nnz,
                          (const double*)A.dval, A.drow, A.dcol,
                          descrB, B.nnz,
                          (const double*)B.dval, B.drow, B.dcol,
                          descrC,
                          (double*)C.dval, C.drow, C.dcol );

        #else
        cusparseDcsrgemm( handle, CUSPARSE_OPERATION_NON_TRANSPOSE,
                          CUSPARSE_OPERATION_NON_TRANSPOSE,
                          A.num_rows, B.num_cols, A.num_cols,
                          descrA, A.nnz,
                          A.dval, A.drow, A.dcol,
                          descrB, B.nnz,
                          B.dval, B.drow, B.dcol,
                          descrC,
                          C.dval, C.drow, C.dcol );
        #endif
        // end CUSPARSE context //
        magma_queue_sync( queue );
        CHECK( magma_dmtransfer( C, AB, Magma_DEV, Magma_DEV, queue ));
    }
    else {
        info = MAGMA_ERR_NOT_SUPPORTED; 
    }
    
cleanup:
    cusparseDestroyMatDescr( descrA );
    cusparseDestroyMatDescr( descrB );
    cusparseDestroyMatDescr( descrC );
    cusparseDestroy( handle );
    magma_dmfree( &C, queue );
    return info;
}
