/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date
       
       @generated from magmablas/zpotf2.cu, normal z -> s, Sat Mar 27 20:31:33 2021
*/
#include "magma_internal.h"

#define REAL

#define sdot_max_bs 512  // 512 is max threads for 1.x cards

void spotf2_sscal( magma_int_t n, float *x, magma_int_t incx, magma_queue_t queue );
void spotf2_sdot(  magma_int_t n, float *x, magma_int_t incx, magma_queue_t queue );

#ifdef COMPLEX
void magmablas_slacgv( magma_int_t n, float *x, magma_int_t incx, magma_queue_t queue );
#endif


// TODO: this function could be in .cpp file -- it has no CUDA code in it.
/***************************************************************************//**
    Purpose
    -------

    spotf2 computes the Cholesky factorization of a real symmetric
    positive definite matrix A.

    The factorization has the form
        A = U**H * U,  if UPLO = MagmaUpper, or
        A = L  * L**H, if UPLO = MagmaLower,
    where U is an upper triangular matrix and L is lower triangular.

    This is the unblocked version of the algorithm, calling Level 2 BLAS.

    Arguments
    ---------

    @param[in]
    uplo    magma_uplo_t
            Specifies whether the upper or lower triangular part of the
            symmetric matrix A is stored.
      -     = MagmaUpper:  Upper triangular
      -     = MagmaLower:  Lower triangular

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0 and N <= 512.

    @param[in,out]
    dA      REAL array, dimension (LDDA,N)
            On entry, the symmetric matrix A.  If UPLO = MagmaUpper, the leading
            n by n upper triangular part of A contains the upper
            triangular part of the matrix A, and the strictly lower
            triangular part of A is not referenced.  If UPLO = MagmaLower, the
            leading n by n lower triangular part of A contains the lower
            triangular part of the matrix A, and the strictly upper
            triangular part of A is not referenced.
    \n
            On exit, if INFO = 0, the factor U or L from the Cholesky
            factorization A = U**H * U  or A = L * L**H.

    @param[in]
    ldda    INTEGER
            The leading dimension of the array A.  LDDA >= max(1,N).

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @param[out]
    info    INTEGER
      -     = 0: successful exit
      -     < 0: if INFO = -k, the k-th argument had an illegal value
      -     > 0: if INFO = k, the leading minor of order k is not
                 positive definite, and the factorization could not be
                 completed.

    @ingroup magma_potf2
*******************************************************************************/
extern "C" magma_int_t
magma_spotf2_gpu(
    magma_uplo_t uplo, magma_int_t n,
    magmaFloat_ptr dA, magma_int_t ldda,
    magma_queue_t queue,
    magma_int_t *info )
{
#define dA(i_, j_)  (dA + (i_) + (j_)*ldda)

    magma_int_t j;

    *info = 0;
    if ( uplo != MagmaUpper && uplo != MagmaLower) {
        *info = -1;
    } else if (n < 0 || n > sdot_max_bs) {
        *info = -2;
    } else if (ldda < max(1,n)) {
        *info = -4;
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info) );
        return *info;
    }

    // Quick return if possible
    if (n == 0) {
        return *info;
    }

    float alpha = MAGMA_S_NEG_ONE;
    float beta  = MAGMA_S_ONE;

    if (uplo == MagmaUpper) {
        for (j = 0; j < n; j++) {
            spotf2_sdot( j, dA(0,j), 1, queue ); // including sdot product and update a(j,j)
            if (j < n) {
                #ifdef COMPLEX
                magmablas_slacgv( j, dA(0, j), 1, queue );
                #endif
                magma_sgemv( MagmaTrans, j, n-j-1,
                             alpha, dA(0, j+1), ldda,
                                    dA(0, j),   1,
                             beta,  dA(j, j+1), ldda, queue );

                #ifdef COMPLEX
                magmablas_slacgv( j, dA(0, j), 1, queue );
                #endif
                spotf2_sscal( n-j, dA(j,j), ldda, queue );
            }
        }
    }
    else {
        for (j = 0; j < n; j++) {
            spotf2_sdot( j, dA(j,0), ldda, queue ); // including sdot product and update a(j,j)
            if (j < n) {
                #ifdef COMPLEX
                magmablas_slacgv( j, dA(j, 0), ldda, queue );
                #endif
                magma_sgemv( MagmaNoTrans, n-j-1, j,
                             alpha, dA(j+1, 0), ldda,
                                    dA(j,0),    ldda,
                             beta,  dA(j+1, j), 1, queue );

                #ifdef COMPLEX
                magmablas_slacgv( j, dA(j, 0), ldda, queue );
                #endif
                spotf2_sscal( n-j, dA(j,j), 1, queue );
            }
        }
    }

    return *info;
}

#define sscal_bs  32
#define sdot_bs  512
#define slacgv_bs 512


__global__ void kernel_sdot(int n, float *x, int incx, int threadSize)
{

    // dynamically allocated shared memory, set to size number of threads when the kernel is launched.
    // See CUDA Guide B.2.3
    extern __shared__ float shared_data[];

    int tx = threadIdx.x;

    float *sdata = shared_data;

    float res = MAGMA_S_ZERO;

    if (tx < n) {
        res = x[tx*incx];
    }

    sdata[tx] = MAGMA_S_REAL(res * MAGMA_S_CONJ(res));

    __syncthreads();

    for (int s = blockDim.x/2; s > 32; s >>= 1 ) {
        if (tx < s) {
            sdata[tx] += sdata[tx+s];
        }
        __syncthreads();
    }

    if (tx < 32) {
        volatile float* smem = sdata;
        smem[tx] += smem[tx+32];
        smem[tx] += smem[tx+16];
        smem[tx] += smem[tx+8];
        smem[tx] += smem[tx+4];
        smem[tx] += smem[tx+2];
        smem[tx] += smem[tx+1];
    }

    if (tx == 0) {
        float xreal = MAGMA_S_REAL(x[n*incx]);
        x[n*incx] = MAGMA_S_MAKE( sqrt(xreal - sdata[0]), 0 );
    }
}

void spotf2_sdot(
    magma_int_t n, float *x, magma_int_t incx,
    magma_queue_t queue )
{
    /*
    Specialized Sdot
    1) performs sdot sum = x[0:n-1]*conj(x[0:n-1])
    2) updates x[n] = sqrt(x[n]-sum);

    */
    if (n > sdot_max_bs) {
        fprintf( stderr, "n = %lld > %lld is not supported in spotf2_sdot\n",
                 (long long) n, (long long) sdot_max_bs );
        return;
    }
    int threadSize;

    if (n <= 1024 && n > 512) {
        threadSize = 1024;
    }
    else if (n <= 512 && n > 256 ) {
        threadSize = 512;
    }
    else if (n <= 256 && n > 128) {
        threadSize = 256;
    }
    else if (n <= 128 && n > 64) {
        threadSize = 128;
    }
    else {
        threadSize = 64;
    }

    size_t shmem = threadSize * sizeof(float);
    kernel_sdot
        <<< 1, threadSize, shmem, queue->cuda_stream() >>>
        (n, x, incx, threadSize);
}

__global__ void kernel_sscal(int n, float *x, int incx)
{
    int id = blockIdx.x * sscal_bs + threadIdx.x;

    __shared__ float factor;

    if (threadIdx.x == 0) {
        factor = MAGMA_S_MAKE(1.0/MAGMA_S_REAL(x[0]), 0.0);
    }

    __syncthreads();

    if ( id < n && id > 0) {
        x[id*incx] = x[id*incx] * factor;
    }
}


void spotf2_sscal(
    magma_int_t n, float *x, magma_int_t incx,
    magma_queue_t queue )
{
    /* Specialized sscal perform x[1:n-1] / x[0] */
    dim3 threads(sscal_bs, 1, 1);
    int num_blocks = magma_ceildiv( n, sscal_bs );
    dim3 grid(num_blocks,1);
    kernel_sscal
        <<< grid, threads, 0, queue->cuda_stream() >>>
        (n, x, incx);
}


#ifdef COMPLEX

__global__ void kernel_slacgv(int n, float *x, int incx)
{
    int id = blockIdx.x * slacgv_bs + threadIdx.x;

    if ( id < n ) {
        x[id*incx] = MAGMA_S_CONJ(x[id*incx]);
    }
}


/***************************************************************************//**
    Purpose
    -------

    SLACGV conjugates a real vector of length N.

    Arguments
    ---------

    @param[in]
    n       INTEGER
            The length of the vector X.  N >= 0.

    @param[in,out]
    x       REAL array, dimension (1+(N-1)*abs(INCX))
            On entry, the vector of length N to be conjugated.
            On exit, X is overwritten with conjg(X).

    @param[in]
    incx    INTEGER
            The spacing between successive elements of X.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_lacgv
*******************************************************************************/
void magmablas_slacgv(
    magma_int_t n, float *x, magma_int_t incx,
    magma_queue_t queue )
{
    dim3 threads(slacgv_bs, 1, 1);
    int num_blocks = magma_ceildiv( n, slacgv_bs );
    dim3 grid(num_blocks,1);
    kernel_slacgv
        <<< grid, threads, 0, queue->cuda_stream() >>>
        (n, x, incx);
}

#endif // COMPLEX
