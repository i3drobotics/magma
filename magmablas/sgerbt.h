/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from magmablas/zgerbt.h, normal z -> s, Sat Mar 27 20:33:21 2021

       @author Adrien Remy
       @author Azzam Haidar
       
       Definitions used in sgerbt.cu sgerbt_batched.cu
*/

#ifndef SGERBT_H
#define SGERBT_H

// =============================================================================
// classical prototypes

__global__ void 
magmablas_selementary_multiplication_kernel(
    magma_int_t n,
    float *dA, magma_int_t offsetA, magma_int_t ldda, 
    float *du, magma_int_t offsetu, 
    float *dv, magma_int_t offsetv);

__global__ void 
magmablas_sapply_vector_kernel(
    magma_int_t n,
    float *du, magma_int_t offsetu,  float *db, magma_int_t offsetb );

__global__ void 
magmablas_sapply_transpose_vector_kernel(
    magma_int_t n,
    float *du, magma_int_t offsetu, float *db, magma_int_t offsetb );

// =============================================================================
// batched prototypes

__global__ void 
magmablas_selementary_multiplication_kernel_batched(
    magma_int_t n,
    float **dA_array, magma_int_t offsetA, magma_int_t ldda, 
    float *du, magma_int_t offsetu, 
    float *dv, magma_int_t offsetv);

__global__ void 
magmablas_sapply_vector_kernel_batched(
    magma_int_t n,
    float *du, magma_int_t offsetu, float **db_array, magma_int_t offsetb );

__global__ void 
magmablas_sapply_transpose_vector_kernel_batched(
    magma_int_t n,
    float *du, magma_int_t offsetu, float **db_array, magma_int_t offsetb );

#endif // SGERBT_H
