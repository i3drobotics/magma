/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from magmablas/ztrtri.cuh, normal z -> d, Sat Mar 27 20:33:20 2021

       @author Peng Du
       @author Tingxing Dong
       @author Mark Gates
       @author Azzam Haidar
       
       Definitions used in dtrtri_diag.cu dtrtri_lower.cu dtrtri_upper.cu
*/

#ifndef DTRTRI_H
#define DTRTRI_H

#define PRECISION_d 

// define 0 for large initializations
#define Z0 MAGMA_D_ZERO

#include "batched_kernel_param.h"
#if   defined(TRTRI_BATCHED)
#define IB    (DTRTRI_BATCHED_BLOCK_SIZE)
#define NB    (DTRTRI_BATCHED_NB)
#elif defined(TRTRI_NONBATCHED)
#define IB    (16)
#define NB    (128)
#else
#error "One of {TRTRI_BATCHED, TRTRI_NONBATCHED} must be defined."
#endif

/*
 * daxpy16 computes c += alpha*b, where b and c are 16-element vectors.
 */
static __device__ void
daxpy16(
    double alpha,
    const double * __restrict__ b,
    double       * __restrict__ c )
{
    c[0]  += alpha * b[0];
    c[1]  += alpha * b[1];
    c[2]  += alpha * b[2];
    c[3]  += alpha * b[3];
    c[4]  += alpha * b[4];
    c[5]  += alpha * b[5];
    c[6]  += alpha * b[6];
    c[7]  += alpha * b[7];
    c[8]  += alpha * b[8];
    c[9]  += alpha * b[9];
    c[10] += alpha * b[10];
    c[11] += alpha * b[11];
    c[12] += alpha * b[12];
    c[13] += alpha * b[13];
    c[14] += alpha * b[14];
    c[15] += alpha * b[15];
}


// unused -- but nearly identical code throughout dtrtri_lower & upper.cu
static __device__ void
dgemm_kernel_16(
    double *A, int lda,
    double *B, int ldb,
    double *C, int ldc,
    double alpha, int jb, int tx, int ty)
{
    const double *Blast = B + jb;
    __shared__ double sB[16][17];
    
    // compute NT x 16 block of C
    // each thread computes one 1x16 row, C(id,0:15)
    double rC[16] = {Z0, Z0, Z0, Z0, Z0, Z0, Z0, Z0, Z0, Z0, Z0, Z0, Z0, Z0, Z0, Z0};
    double rA[4];

    do {
        // load 16 x 16 block of B using NX x NY threads
        #pragma unroll
        for( int i=0; i < 16; i += blockDim.x ) {
            #pragma unroll
            for( int j=0; j < 16; j += blockDim.y ) {
                sB[tx + i][ty + j] = B[i + j*ldb];
            }
        }
        __syncthreads();
        
        // load NT x 16 block of A; each thread initially loads 1x4 row,
        // then continues loading more elements as axpys are done.
        rA[0] = A[0*lda];
        rA[1] = A[1*lda];
        rA[2] = A[2*lda];
        rA[3] = A[3*lda];

        // axpy:  C(id,:) += A(id,k) * B(k,:) for k={0,0}, ..., 15
        daxpy16( rA[0], &sB[ 0][0], rC );  rA[0] = A[ 4*lda];
        daxpy16( rA[1], &sB[ 1][0], rC );  rA[1] = A[ 5*lda];
        daxpy16( rA[2], &sB[ 2][0], rC );  rA[2] = A[ 6*lda];
        daxpy16( rA[3], &sB[ 3][0], rC );  rA[3] = A[ 7*lda];
        
        daxpy16( rA[0], &sB[ 4][0], rC );  rA[0] = A[ 8*lda];
        daxpy16( rA[1], &sB[ 5][0], rC );  rA[1] = A[ 9*lda];
        daxpy16( rA[2], &sB[ 6][0], rC );  rA[2] = A[10*lda];
        daxpy16( rA[3], &sB[ 7][0], rC );  rA[3] = A[11*lda];

        daxpy16( rA[0], &sB[ 8][0], rC );  rA[0] = A[12*lda];
        daxpy16( rA[1], &sB[ 9][0], rC );  rA[1] = A[13*lda];
        daxpy16( rA[2], &sB[10][0], rC );  rA[2] = A[14*lda];
        daxpy16( rA[3], &sB[11][0], rC );  rA[3] = A[15*lda];

        daxpy16( rA[0], &sB[12][0], rC );
        daxpy16( rA[1], &sB[13][0], rC );
        daxpy16( rA[2], &sB[14][0], rC );
        daxpy16( rA[3], &sB[15][0], rC );

        // move to next block of A and B
        A += 16*lda;
        B += 16;
        __syncthreads();
    } while( B < Blast );

    // write NT x 16 result; each thread writes one 16x1 row, C(id,0:15)
    for( int i = 0; i < 16; i++ ) {
        C[0] = alpha*rC[i];
        C += ldc;
    }
}


__global__ void
dtrtri_diag_lower_kernel(
    magma_diag_t diag, int n, const double *A, int lda, double *d_invA);

__global__ void
triple_dgemm16_part1_lower_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm16_part2_lower_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm32_part1_lower_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm32_part2_lower_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm64_part1_lower_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm64_part2_lower_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm_above64_part1_lower_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm_above64_part2_lower_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm_above64_part3_lower_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);


    
__global__ void
dtrtri_diag_upper_kernel(
    magma_diag_t diag, int n, const double *A, int lda, double *d_invA);

__global__ void
triple_dgemm16_part1_upper_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm16_part2_upper_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm32_part1_upper_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm32_part2_upper_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm64_part1_upper_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm64_part2_upper_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm_above64_part1_upper_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm_above64_part2_upper_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);

__global__ void
triple_dgemm_above64_part3_upper_kernel(
    int n, const double *Ain, int lda, double *d_invA, int jb, int npages);







__global__ void
dtrtri_diag_lower_kernel_batched(
    magma_diag_t diag, int n, double const * const * dA_array, int lda, double **dinvA_array);

__global__ void
triple_dgemm16_part1_lower_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm16_part2_lower_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm32_part1_lower_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm32_part2_lower_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm64_part1_lower_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm64_part2_lower_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm_above64_part1_lower_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm_above64_part2_lower_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm_above64_part3_lower_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);


    
__global__ void
dtrtri_diag_upper_kernel_batched(
    magma_diag_t diag, int n, double const * const * dA_array, int lda, double **dinvA_array);

__global__ void
triple_dgemm16_part1_upper_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm16_part2_upper_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm32_part1_upper_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm32_part2_upper_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm64_part1_upper_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm64_part2_upper_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm_above64_part1_upper_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm_above64_part2_upper_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm_above64_part3_upper_kernel_batched(
    int n, double const * const * Ain_array, int lda, double **dinvA_array, int jb, int npages);



/* vbatched kernels */
////////////////////////////////////////////////////////////////////////////////////////////////////
__global__ void
dtrtri_diag_lower_kernel_vbatched(
    magma_diag_t diag, magma_int_t* n, double const * const * dA_array, magma_int_t* lda, double **dinvA_array);

__global__ void
triple_dgemm16_part1_lower_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm16_part2_lower_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm32_part1_lower_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm32_part2_lower_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm64_part1_lower_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm64_part2_lower_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm_above64_part1_lower_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm_above64_part2_lower_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm_above64_part3_lower_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);



__global__ void
dtrtri_diag_upper_kernel_vbatched(
    magma_diag_t diag, magma_int_t* n, double const * const * dA_array, magma_int_t* lda, double **dinvA_array);

__global__ void
triple_dgemm16_part1_upper_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm16_part2_upper_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm32_part1_upper_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm32_part2_upper_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm64_part1_upper_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm64_part2_upper_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm_above64_part1_upper_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm_above64_part2_upper_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

__global__ void
triple_dgemm_above64_part3_upper_kernel_vbatched(
    magma_int_t* n, double const * const * Ain_array, magma_int_t* lda, double **dinvA_array, int jb, int npages);

#endif // DTRTRI_H
