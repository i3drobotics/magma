/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/blas/magma_zcuspaxpy.cpp, normal z -> d, Sat Mar 27 20:32:31 2021
       @author Hartwig Anzt

*/
#include "magmasparse_internal.h"

#define RTOLERANCE     lapackf77_dlamch( "E" )
#define ATOLERANCE     lapackf77_dlamch( "E" )

// todo: alpha and beta passed, dvalA, dvalB, valC, colC; buf passed to next
#if CUDA_VERSION >= 11000
#define cusparseXcsrgeamNnz(handle, m, n, descrA, nnzA, drowA, dcolA,                           \
                            descrB, nnzB, drowB, dcolB, descrC, rowC, nnzTotal )                \
    {                                                                                           \
        size_t bufsize;                                                                         \
        void *buf;                                                                              \
        double alpha = MAGMA_D_ONE, beta = MAGMA_D_ZERO;                               \
        cusparseSetPointerMode(handle, CUSPARSE_POINTER_MODE_HOST);                             \
        cusparseDcsrgeam2_bufferSizeExt(handle, m, n, &alpha,                                   \
                                        descrA, nnzA, NULL, drowA, dcolA,                       \
                                        &beta,                                                  \
                                        descrB, nnzB, NULL, drowB, dcolB,                       \
                                        descrC, NULL, rowC, NULL, &bufsize);                    \
        if (bufsize > 0)                                                                        \
           magma_malloc(&buf, bufsize);                                                         \
        cusparseXcsrgeam2Nnz(handle, m, n,                                                      \
                             descrA, nnzA, drowA, dcolA,                                        \
                             descrB, nnzB, drowB, dcolB,                                        \
                             descrC, rowC, nnzTotal, buf);                                      \
        if (bufsize > 0)                                                                        \
           magma_free(buf);                                                                     \
    }
#endif

// todo: buf is passed from above
#if CUDA_VERSION >= 11000
#define cusparseDcsrgeam(handle, m, n, alpha, descrA, nnzA, dvalA, drowA, dcolA, beta,          \
                         descrB, nnzB, dvalB, drowB, dcolB, descrC, dvalC, drowC, dcolC )       \
    {                                                                                           \
        void *buf;                                                                              \
        printf("cusparseDcsrgeam bufsize = %d\n", 0);\
        cusparseDcsrgeam2(handle, m, n, alpha,                                                  \
                          descrA, nnzA, dvalA, drowA, dcolA,                                    \
                          beta,                                                                 \
                          descrB, nnzB, dvalB, drowB, dcolB,                                    \
                          descrC, dvalC, drowC, dcolC,                                          \
                          buf);                                                                 \
    }
#endif

/**
    Purpose
    -------

    This is an interface to the cuSPARSE routine csrgeam computing the sum
    of two sparse matrices stored in csr format:

        C = alpha * A + beta * B


    Arguments
    ---------

    @param[in]
    alpha       double*
                scalar

    @param[in]
    A           magma_d_matrix
                input matrix

    @param[in]
    beta        double*
                scalar

    @param[in]
    B           magma_d_matrix
                input matrix

    @param[out]
    AB          magma_d_matrix*
                output matrix AB = alpha * A + beta * B

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_dblas
    ********************************************************************/

extern "C" magma_int_t
magma_dcuspaxpy(
    double *alpha, magma_d_matrix A,
    double *beta, magma_d_matrix B,
    magma_d_matrix *AB,
    magma_queue_t queue )
{
    magma_int_t info = 0;
    
    magma_d_matrix C={Magma_CSR};
    C.num_rows = A.num_rows;
    C.num_cols = A.num_cols;
    C.storage_type = A.storage_type;
    C.memory_location = A.memory_location;
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
        // CUSPARSE context //
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
        cusparseXcsrgeamNnz( handle, A.num_rows, A.num_cols,
                             descrA, A.nnz, A.drow, A.dcol,
                             descrB, B.nnz, B.drow, B.dcol,
                             descrC, C.row, nnzTotalDevHostPtr );

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
        hipsparseDcsrgeam( handle, A.num_rows, A.num_cols,
                          (const double*)alpha,
                          descrA, A.nnz,
                          (const double*)A.dval, A.drow, A.dcol,
                          (const double*)beta,
                          descrB, B.nnz,
                          (const double*)B.dval, B.drow, B.dcol,
                          descrC,
                          (double*)C.dval, C.drow, C.dcol );
        #else
        cusparseDcsrgeam( handle, A.num_rows, A.num_cols,
                          alpha,
                          descrA, A.nnz,
                          A.dval, A.drow, A.dcol,
                          beta,
                          descrB, B.nnz,
                          B.dval, B.drow, B.dcol,
                          descrC,
                          C.dval, C.drow, C.dcol );
        #endif

        // end CUSPARSE context //

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
