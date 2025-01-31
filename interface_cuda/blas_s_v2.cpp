/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Mark Gates
       @generated from interface_cuda/blas_z_v2.cpp, normal z -> s, Sat Mar 27 20:31:15 2021
*/
#include "magma_internal.h"
#include "error.h"

#define REAL

#define PRECISION_s

/* on some platforms (i.e. hipMAGMA on ROCm stack), we define custom types
 * So, to keep the C++ compiler from giving errors, we cast arguments to internal
 * BLAS routines. The hipify script should replace `cu*Complex` with appropriate HIP types
 *
 * FUTURE READERS: If hipBLAS changes numbers to `hipblas*Complex` rather than `hip*Complex`,
 *   these will need more complicated macro if/else blocks
 */
/*#ifdef PRECISION_z
  #ifdef HAVE_HIP
    typedef float float;
  #else
    typedef float float;
  #endif
#elif defined(PRECISION_c)
  #ifdef HAVE_HIP
    typedef hipComplex float;
  #else
    typedef cuFloatComplex float;
  #endif
#elif defined(PRECISION_d)
  typedef float float;
#else
  typedef float float;
#endif
*/
//#ifdef HAVE_CUBLAS

// =============================================================================
// Level 1 BLAS

/***************************************************************************//**
    @return Index of element of vector x having max. absolute value;
            \f$ \text{argmax}_i\; | real(x_i) | + | imag(x_i) | \f$.

    @param[in]
    n       Number of elements in vector x. n >= 0.

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx > 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_iamax
*******************************************************************************/
extern "C" magma_int_t
magma_isamax(
    magma_int_t n,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magma_queue_t queue )
{
    int result; /* not magma_int_t */
    cublasIsamax( queue->cublas_handle(), int(n), (float*)dx, int(incx), &result );
    return result;
}


/***************************************************************************//**
    @return Index of element of vector x having min. absolute value;
            \f$ \text{argmin}_i\; | real(x_i) | + | imag(x_i) | \f$.

    @param[in]
    n       Number of elements in vector x. n >= 0.

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx > 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_iamin
*******************************************************************************/
extern "C" magma_int_t
magma_isamin(
    magma_int_t n,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magma_queue_t queue )
{
    int result; /* not magma_int_t */
    cublasIsamin( queue->cublas_handle(), int(n), (float*)dx, int(incx), &result );
    return result;
}


/***************************************************************************//**
    @return Sum of absolute values of vector x;
            \f$ \sum_i | real(x_i) | + | imag(x_i) | \f$.

    @param[in]
    n       Number of elements in vector x. n >= 0.

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx > 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_asum
*******************************************************************************/
extern "C" float
magma_sasum(
    magma_int_t n,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magma_queue_t queue )
{
    float result;
    cublasSasum( queue->cublas_handle(), int(n), (float*)dx, int(incx), &result );
    return result;
}


/***************************************************************************//**
    Constant times a vector plus a vector; \f$ y = \alpha x + y \f$.

    @param[in]
    n       Number of elements in vectors x and y. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in,out]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_axpy
*******************************************************************************/
extern "C" void
magma_saxpy(
    magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magmaFloat_ptr       dy, magma_int_t incy,
    magma_queue_t queue )
{
    cublasSaxpy( queue->cublas_handle(), int(n), (float*)&alpha, (float*)dx, int(incx), (float*)dy, int(incy) );
}


/***************************************************************************//**
    Copy vector x to vector y; \f$ y = x \f$.

    @param[in]
    n       Number of elements in vectors x and y. n >= 0.

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[out]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_copy
*******************************************************************************/
extern "C" void
magma_scopy(
    magma_int_t n,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magmaFloat_ptr       dy, magma_int_t incy,
    magma_queue_t queue )
{
    cublasScopy( queue->cublas_handle(), int(n), (float*)dx, int(incx), (float*)dy, int(incy) );
}


#ifdef COMPLEX
/***************************************************************************//**
    @return Dot product of vectors x and y; \f$ x^H y \f$.

    @param[in]
    n       Number of elements in vector x and y. n >= 0.

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma__dot
*******************************************************************************/
extern "C"
float magma_sdot(
    magma_int_t n,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magmaFloat_const_ptr dy, magma_int_t incy,
    magma_queue_t queue )
{
    float result;
    cublasSdot( queue->cublas_handle(), int(n), (float*)dx, int(incx), (float*)dy, int(incy), (float*)&result );
    return result;
}
#endif // COMPLEX


/***************************************************************************//**
    @return Dot product (unconjugated) of vectors x and y; \f$ x^T y \f$.

    @param[in]
    n       Number of elements in vector x and y. n >= 0.

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma__dot
*******************************************************************************/
extern "C"
float magma_sdot(
    magma_int_t n,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magmaFloat_const_ptr dy, magma_int_t incy,
    magma_queue_t queue )
{
    float result;
    cublasSdot( queue->cublas_handle(), int(n), (float*)dx, int(incx), (float*)dy, int(incy), (float*)&result );
    return result;
}


/***************************************************************************//**
    @return 2-norm of vector x; \f$ \text{sqrt}( x^H x ) \f$.
            Avoids unnecesary over/underflow.

    @param[in]
    n       Number of elements in vector x and y. n >= 0.

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx > 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_nrm2
*******************************************************************************/
extern "C" float
magma_snrm2(
    magma_int_t n,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magma_queue_t queue )
{
    float result;
    cublasSnrm2( queue->cublas_handle(), int(n), (float*)dx, int(incx), &result );
    return result;
}


/***************************************************************************//**
    Apply Givens plane rotation, where cos (c) is real and sin (s) is real.

    @param[in]
    n       Number of elements in vector x and y. n >= 0.

    @param[in,out]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).
            On output, overwritten with c*x + s*y.

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in,out]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).
            On output, overwritten with -conj(s)*x + c*y.

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in]
    c       float. cosine.

    @param[in]
    s       REAL. sine. c and s define a rotation
            [ c         s ]  where c*c + s*conj(s) = 1.
            [ -conj(s)  c ]

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_rot
*******************************************************************************/
extern "C" void
magma_srot(
    magma_int_t n,
    magmaFloat_ptr dx, magma_int_t incx,
    magmaFloat_ptr dy, magma_int_t incy,
    float c, float s,
    magma_queue_t queue )
{
    cublasSrot( queue->cublas_handle(), int(n), (float*)dx, int(incx), (float*)dy, int(incy), &c, (float*)&s );
}


#ifdef COMPLEX
/***************************************************************************//**
    Apply Givens plane rotation, where cos (c) and sin (s) are real.

    @param[in]
    n       Number of elements in vector x and y. n >= 0.

    @param[in,out]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).
            On output, overwritten with c*x + s*y.

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in,out]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).
            On output, overwritten with -conj(s)*x + c*y.

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in]
    c       float. cosine.

    @param[in]
    s       float. sine. c and s define a rotation
            [  c  s ]  where c*c + s*s = 1.
            [ -s  c ]

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_rot
*******************************************************************************/
extern "C" void
magma_srot(
    magma_int_t n,
    magmaFloat_ptr dx, magma_int_t incx,
    magmaFloat_ptr dy, magma_int_t incy,
    float c, float s,
    magma_queue_t queue )
{
    cublasSrot( queue->cublas_handle(), int(n), (float*)dx, int(incx), (float*)dy, int(incy), &c, &s );
}
#endif // COMPLEX


/***************************************************************************//**
    Generate a Givens plane rotation.
    The rotation annihilates the second entry of the vector, such that:

        (  c  s ) * ( a ) = ( r )
        ( -s  c )   ( b )   ( 0 )

    where \f$ c^2 + s^2 = 1 \f$ and \f$ r = a^2 + b^2 \f$.
    Further, this computes z such that

                { (sqrt(1 - z^2), z),    if |z| < 1,
        (c,s) = { (0, 1),                if |z| = 1,
                { (1/z, sqrt(1 - z^2)),  if |z| > 1.

    @param[in]
    a       On input, entry to be modified.
            On output, updated to r by applying the rotation.

    @param[in,out]
    b       On input, entry to be annihilated.
            On output, set to z.

    @param[in]
    c       On output, cosine of rotation.

    @param[in,out]
    s       On output, sine of rotation.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_rotg
*******************************************************************************/
extern "C" void
magma_srotg(
    float *a, float *b,
    float             *c, float *s,
    magma_queue_t queue )
{
    cublasSrotg( queue->cublas_handle(), (float*)a, (float*)b, c, (float*)s );
}


#ifdef REAL
/***************************************************************************//**
    Apply modified plane rotation.

    @ingroup magma_rotm
*******************************************************************************/
extern "C" void
magma_srotm(
    magma_int_t n,
    float *dx, magma_int_t incx,
    float *dy, magma_int_t incy,
    const float *param,
    magma_queue_t queue )
{
    cublasSrotm( queue->cublas_handle(), int(n), dx, int(incx), dy, int(incy), param );
}
#endif // REAL


#ifdef REAL
/***************************************************************************//**
    Generate modified plane rotation.

    @ingroup magma_rotmg
*******************************************************************************/
extern "C" void
magma_srotmg(
    float *d1, float       *d2,
    float *x1, const float *y1,
    float *param,
    magma_queue_t queue )
{
    cublasSrotmg( queue->cublas_handle(), d1, d2, x1, y1, param );
}
#endif // REAL


/***************************************************************************//**
    Scales a vector by a constant; \f$ x = \alpha x \f$.

    @param[in]
    n       Number of elements in vector x. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in,out]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx > 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_scal
*******************************************************************************/
extern "C" void
magma_sscal(
    magma_int_t n,
    float alpha,
    magmaFloat_ptr dx, magma_int_t incx,
    magma_queue_t queue )
{
    cublasSscal( queue->cublas_handle(), int(n), (float*)&alpha, (float*)dx, int(incx) );
}


#ifdef COMPLEX
/***************************************************************************//**
    Scales a vector by a real constant; \f$ x = \alpha x \f$.

    @param[in]
    n       Number of elements in vector x. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$ (real)

    @param[in,out]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx > 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_scal
*******************************************************************************/
extern "C" void
magma_sscal(
    magma_int_t n,
    float alpha,
    magmaFloat_ptr dx, magma_int_t incx,
    magma_queue_t queue )
{
    cublasSscal( queue->cublas_handle(), int(n), &alpha, (float*)dx, int(incx) );
}
#endif // COMPLEX


/***************************************************************************//**
    Swap vector x and y; \f$ x <-> y \f$.

    @param[in]
    n       Number of elements in vector x and y. n >= 0.

    @param[in,out]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in,out]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_swap
*******************************************************************************/
extern "C" void
magma_sswap(
    magma_int_t n,
    magmaFloat_ptr dx, magma_int_t incx,
    magmaFloat_ptr dy, magma_int_t incy,
    magma_queue_t queue )
{
    cublasSswap( queue->cublas_handle(), int(n), (float*)dx, int(incx), (float*)dy, int(incy) );
}


// =============================================================================
// Level 2 BLAS

/***************************************************************************//**
    Perform matrix-vector product.
        \f$ y = \alpha A   x + \beta y \f$  (transA == MagmaNoTrans), or \n
        \f$ y = \alpha A^T x + \beta y \f$  (transA == MagmaTrans),   or \n
        \f$ y = \alpha A^H x + \beta y \f$  (transA == MagmaConjTrans).

    @param[in]
    transA  Operation to perform on A.

    @param[in]
    m       Number of rows of A. m >= 0.

    @param[in]
    n       Number of columns of A. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dA      REAL array of dimension (ldda,n), ldda >= max(1,m).
            The m-by-n matrix A, on GPU device.

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    dx      REAL array on GPU device.
            If transA == MagmaNoTrans, the n element vector x of dimension (1 + (n-1)*incx); \n
            otherwise,                 the m element vector x of dimension (1 + (m-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in]
    beta    Scalar \f$ \beta \f$

    @param[in,out]
    dy      REAL array on GPU device.
            If transA == MagmaNoTrans, the m element vector y of dimension (1 + (m-1)*incy); \n
            otherwise,                 the n element vector y of dimension (1 + (n-1)*incy).

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_gemv
*******************************************************************************/
extern "C" void
magma_sgemv(
    magma_trans_t transA,
    magma_int_t m, magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magmaFloat_const_ptr dx, magma_int_t incx,
    float beta,
    magmaFloat_ptr       dy, magma_int_t incy,
    magma_queue_t queue )
{
    cublasSgemv(
        queue->cublas_handle(),
        cublas_trans_const( transA ),
        int(m), int(n),
        (float*)&alpha, (float*)dA, int(ldda),
                (float*)dx, int(incx),
        (float*)&beta,  (float*)dy, int(incy) );
}


#ifdef COMPLEX
/***************************************************************************//**
    Perform rank-1 update, \f$ A = \alpha x y^H + A \f$.

    @param[in]
    m       Number of rows of A. m >= 0.

    @param[in]
    n       Number of columns of A. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dx      REAL array on GPU device.
            The m element vector x of dimension (1 + (m-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in,out]
    dA      REAL array on GPU device.
            The m-by-n matrix A of dimension (ldda,n), ldda >= max(1,m).

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_ger
*******************************************************************************/
extern "C" void
magma_sger(
    magma_int_t m, magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magmaFloat_const_ptr dy, magma_int_t incy,
    magmaFloat_ptr       dA, magma_int_t ldda,
    magma_queue_t queue )
{
    cublasSger(
        queue->cublas_handle(),
        int(m), int(n),
        (float*)&alpha, (float*)dx, int(incx),
                (float*)dy, int(incy),
                (float*)dA, int(ldda) );
}
#endif // COMPLEX


/***************************************************************************//**
    Perform rank-1 update (unconjugated), \f$ A = \alpha x y^T + A \f$.

    @param[in]
    m       Number of rows of A. m >= 0.

    @param[in]
    n       Number of columns of A. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dx      REAL array on GPU device.
            The m element vector x of dimension (1 + (m-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in,out]
    dA      REAL array of dimension (ldda,n), ldda >= max(1,m).
            The m-by-n matrix A, on GPU device.

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_ger
*******************************************************************************/
extern "C" void
magma_sger(
    magma_int_t m, magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magmaFloat_const_ptr dy, magma_int_t incy,
    magmaFloat_ptr       dA, magma_int_t ldda,
    magma_queue_t queue )
{
    cublasSger(
        queue->cublas_handle(),
        int(m), int(n),
        (float*)&alpha, (float*)dx, int(incx),
                (float*)dy, int(incy),
                (float*)dA, int(ldda) );
}


#ifdef COMPLEX
/***************************************************************************//**
    Perform symmetric matrix-vector product, \f$ y = \alpha A x + \beta y, \f$
    where \f$ A \f$ is symmetric.

    @param[in]
    uplo    Whether the upper or lower triangle of A is referenced.

    @param[in]
    n       Number of rows and columns of A. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dA      REAL array of dimension (ldda,n), ldda >= max(1,n).
            The n-by-n matrix A, on GPU device.

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    dx      REAL array on GPU device.
            The m element vector x of dimension (1 + (m-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in]
    beta    Scalar \f$ \beta \f$

    @param[in,out]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_hemv
*******************************************************************************/
extern "C" void
magma_ssymv(
    magma_uplo_t uplo,
    magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magmaFloat_const_ptr dx, magma_int_t incx,
    float beta,
    magmaFloat_ptr       dy, magma_int_t incy,
    magma_queue_t queue )
{
    cublasSsymv(
        queue->cublas_handle(),
        cublas_uplo_const( uplo ),
        int(n),
        (float*)&alpha, (float*)dA, int(ldda),
                (float*)dx, int(incx),
        (float*)&beta,  (float*)dy, int(incy) );
}
#endif // COMPLEX


#ifdef COMPLEX
/***************************************************************************//**
    Perform symmetric rank-1 update, \f$ A = \alpha x x^H + A, \f$
    where \f$ A \f$ is symmetric.

    @param[in]
    uplo    Whether the upper or lower triangle of A is referenced.

    @param[in]
    n       Number of rows and columns of A. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in,out]
    dA      REAL array of dimension (ldda,n), ldda >= max(1,n).
            The n-by-n matrix A, on GPU device.

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_her
*******************************************************************************/
extern "C" void
magma_ssyr(
    magma_uplo_t uplo,
    magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magmaFloat_ptr       dA, magma_int_t ldda,
    magma_queue_t queue )
{
    cublasSsyr(
        queue->cublas_handle(),
        cublas_uplo_const( uplo ),
        int(n),
        (const float*)&alpha, (float*)dx, int(incx),
                (float*)dA, int(ldda) );
}
#endif // COMPLEX


#ifdef COMPLEX
/***************************************************************************//**
    Perform symmetric rank-2 update, \f$ A = \alpha x y^H + conj(\alpha) y x^H + A, \f$
    where \f$ A \f$ is symmetric.

    @param[in]
    uplo    Whether the upper or lower triangle of A is referenced.

    @param[in]
    n       Number of rows and columns of A. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in,out]
    dA      REAL array of dimension (ldda,n), ldda >= max(1,n).
            The n-by-n matrix A, on GPU device.

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_her2
*******************************************************************************/
extern "C" void
magma_ssyr2(
    magma_uplo_t uplo,
    magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magmaFloat_const_ptr dy, magma_int_t incy,
    magmaFloat_ptr       dA, magma_int_t ldda,
    magma_queue_t queue )
{
    cublasSsyr2(
        queue->cublas_handle(),
        cublas_uplo_const( uplo ),
        int(n),
        (float*)&alpha, (float*)dx, int(incx),
                (float*)dy, int(incy),
                (float*)dA, int(ldda) );
}
#endif // COMPLEX


/***************************************************************************//**
    Perform symmetric matrix-vector product, \f$ y = \alpha A x + \beta y, \f$
    where \f$ A \f$ is symmetric.

    @param[in]
    uplo    Whether the upper or lower triangle of A is referenced.

    @param[in]
    n       Number of rows and columns of A. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dA      REAL array of dimension (ldda,n), ldda >= max(1,n).
            The n-by-n matrix A, on GPU device.

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    dx      REAL array on GPU device.
            The m element vector x of dimension (1 + (m-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in]
    beta    Scalar \f$ \beta \f$

    @param[in,out]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_symv
*******************************************************************************/
extern "C" void
magma_ssymv(
    magma_uplo_t uplo,
    magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magmaFloat_const_ptr dx, magma_int_t incx,
    float beta,
    magmaFloat_ptr       dy, magma_int_t incy,
    magma_queue_t queue )
{
    cublasSsymv(
        queue->cublas_handle(),
        cublas_uplo_const( uplo ),
        int(n),
        (float*)&alpha, (float*)dA, int(ldda),
                (float*)dx, int(incx),
        (float*)&beta,  (float*)dy, int(incy) );
}


/***************************************************************************//**
    Perform symmetric rank-1 update, \f$ A = \alpha x x^T + A, \f$
    where \f$ A \f$ is symmetric.

    @param[in]
    uplo    Whether the upper or lower triangle of A is referenced.

    @param[in]
    n       Number of rows and columns of A. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in,out]
    dA      REAL array of dimension (ldda,n), ldda >= max(1,n).
            The n-by-n matrix A, on GPU device.

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_syr
*******************************************************************************/
extern "C" void
magma_ssyr(
    magma_uplo_t uplo,
    magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magmaFloat_ptr       dA, magma_int_t ldda,
    magma_queue_t queue )
{
    cublasSsyr(
        queue->cublas_handle(),
        cublas_uplo_const( uplo ),
        int(n),
        (float*)&alpha, (float*)dx, int(incx),
                (float*)dA, int(ldda) );
}


/***************************************************************************//**
    Perform symmetric rank-2 update, \f$ A = \alpha x y^T + \alpha y x^T + A, \f$
    where \f$ A \f$ is symmetric.

    @param[in]
    uplo    Whether the upper or lower triangle of A is referenced.

    @param[in]
    n       Number of rows and columns of A. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in]
    dy      REAL array on GPU device.
            The n element vector y of dimension (1 + (n-1)*incy).

    @param[in]
    incy    Stride between consecutive elements of dy. incy != 0.

    @param[in,out]
    dA      REAL array of dimension (ldda,n), ldda >= max(1,n).
            The n-by-n matrix A, on GPU device.

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_syr2
*******************************************************************************/
extern "C" void
magma_ssyr2(
    magma_uplo_t uplo,
    magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dx, magma_int_t incx,
    magmaFloat_const_ptr dy, magma_int_t incy,
    magmaFloat_ptr       dA, magma_int_t ldda,
    magma_queue_t queue )
{
    cublasSsyr2(
        queue->cublas_handle(),
        cublas_uplo_const( uplo ),
        int(n),
        (float*)&alpha, (float*)dx, int(incx),
                (float*)dy, int(incy),
                (float*)dA, int(ldda) );
}


/***************************************************************************//**
    Perform triangular matrix-vector product.
        \f$ x = A   x \f$  (trans == MagmaNoTrans), or \n
        \f$ x = A^T x \f$  (trans == MagmaTrans),   or \n
        \f$ x = A^H x \f$  (trans == MagmaConjTrans).

    @param[in]
    uplo    Whether the upper or lower triangle of A is referenced.

    @param[in]
    trans   Operation to perform on A.

    @param[in]
    diag    Whether the diagonal of A is assumed to be unit or non-unit.

    @param[in]
    n       Number of rows and columns of A. n >= 0.

    @param[in]
    dA      REAL array of dimension (ldda,n), ldda >= max(1,n).
            The n-by-n matrix A, on GPU device.

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    dx      REAL array on GPU device.
            The n element vector x of dimension (1 + (n-1)*incx).

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_trmv
*******************************************************************************/
extern "C" void
magma_strmv(
    magma_uplo_t uplo, magma_trans_t trans, magma_diag_t diag,
    magma_int_t n,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magmaFloat_ptr       dx, magma_int_t incx,
    magma_queue_t queue )
{
    cublasStrmv(
        queue->cublas_handle(),
        cublas_uplo_const( uplo ),
        cublas_trans_const( trans ),
        cublas_diag_const( diag ),
        int(n),
        (float*)dA, int(ldda),
        (float*)dx, int(incx) );
}


/***************************************************************************//**
    Solve triangular matrix-vector system (one right-hand side).
        \f$ A   x = b \f$  (trans == MagmaNoTrans), or \n
        \f$ A^T x = b \f$  (trans == MagmaTrans),   or \n
        \f$ A^H x = b \f$  (trans == MagmaConjTrans).

    @param[in]
    uplo    Whether the upper or lower triangle of A is referenced.

    @param[in]
    trans   Operation to perform on A.

    @param[in]
    diag    Whether the diagonal of A is assumed to be unit or non-unit.

    @param[in]
    n       Number of rows and columns of A. n >= 0.

    @param[in]
    dA      REAL array of dimension (ldda,n), ldda >= max(1,n).
            The n-by-n matrix A, on GPU device.

    @param[in]
    ldda    Leading dimension of dA.

    @param[in,out]
    dx      REAL array on GPU device.
            On entry, the n element RHS vector b of dimension (1 + (n-1)*incx).
            On exit, overwritten with the solution vector x.

    @param[in]
    incx    Stride between consecutive elements of dx. incx != 0.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_trsv
*******************************************************************************/
extern "C" void
magma_strsv(
    magma_uplo_t uplo, magma_trans_t trans, magma_diag_t diag,
    magma_int_t n,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magmaFloat_ptr       dx, magma_int_t incx,
    magma_queue_t queue )
{
    cublasStrsv(
        queue->cublas_handle(),
        cublas_uplo_const( uplo ),
        cublas_trans_const( trans ),
        cublas_diag_const( diag ),
        int(n),
        (float*)dA, int(ldda),
        (float*)dx, int(incx) );
}


// =============================================================================
// Level 3 BLAS

/***************************************************************************//**
    Perform matrix-matrix product, \f$ C = \alpha op(A) op(B) + \beta C \f$.

    @param[in]
    transA  Operation op(A) to perform on matrix A.

    @param[in]
    transB  Operation op(B) to perform on matrix B.

    @param[in]
    m       Number of rows of C and op(A). m >= 0.

    @param[in]
    n       Number of columns of C and op(B). n >= 0.

    @param[in]
    k       Number of columns of op(A) and rows of op(B). k >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dA      REAL array on GPU device.
            If transA == MagmaNoTrans, the m-by-k matrix A of dimension (ldda,k), ldda >= max(1,m); \n
            otherwise,                 the k-by-m matrix A of dimension (ldda,m), ldda >= max(1,k).

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    dB      REAL array on GPU device.
            If transB == MagmaNoTrans, the k-by-n matrix B of dimension (lddb,n), lddb >= max(1,k); \n
            otherwise,                 the n-by-k matrix B of dimension (lddb,k), lddb >= max(1,n).

    @param[in]
    lddb    Leading dimension of dB.

    @param[in]
    beta    Scalar \f$ \beta \f$

    @param[in,out]
    dC      REAL array on GPU device.
            The m-by-n matrix C of dimension (lddc,n), lddc >= max(1,m).

    @param[in]
    lddc    Leading dimension of dC.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_gemm
*******************************************************************************/
extern "C" void
magma_sgemm(
    magma_trans_t transA, magma_trans_t transB,
    magma_int_t m, magma_int_t n, magma_int_t k,
    float alpha,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magmaFloat_const_ptr dB, magma_int_t lddb,
    float beta,
    magmaFloat_ptr       dC, magma_int_t lddc,
    magma_queue_t queue )
{
    cublasSgemm(
        queue->cublas_handle(),
        cublas_trans_const( transA ),
        cublas_trans_const( transB ),
        int(m), int(n), int(k),
        (float*)&alpha, (float*)dA, int(ldda),
                (float*)dB, int(lddb),
        (float*)&beta,  (float*)dC, int(lddc) );
}


#ifdef COMPLEX
/***************************************************************************//**
    Perform symmetric matrix-matrix product.
        \f$ C = \alpha A B + \beta C \f$ (side == MagmaLeft), or \n
        \f$ C = \alpha B A + \beta C \f$ (side == MagmaRight),   \n
    where \f$ A \f$ is symmetric.

    @param[in]
    side    Whether A is on the left or right.

    @param[in]
    uplo    Whether the upper or lower triangle of A is referenced.

    @param[in]
    m       Number of rows of C. m >= 0.

    @param[in]
    n       Number of columns of C. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dA      REAL array on GPU device.
            If side == MagmaLeft, the m-by-m symmetric matrix A of dimension (ldda,m), ldda >= max(1,m); \n
            otherwise,            the n-by-n symmetric matrix A of dimension (ldda,n), ldda >= max(1,n).

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    dB      REAL array on GPU device.
            The m-by-n matrix B of dimension (lddb,n), lddb >= max(1,m).

    @param[in]
    lddb    Leading dimension of dB.

    @param[in]
    beta    Scalar \f$ \beta \f$

    @param[in,out]
    dC      REAL array on GPU device.
            The m-by-n matrix C of dimension (lddc,n), lddc >= max(1,m).

    @param[in]
    lddc    Leading dimension of dC.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_hemm
*******************************************************************************/
extern "C" void
magma_ssymm(
    magma_side_t side, magma_uplo_t uplo,
    magma_int_t m, magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magmaFloat_const_ptr dB, magma_int_t lddb,
    float beta,
    magmaFloat_ptr       dC, magma_int_t lddc,
    magma_queue_t queue )
{
    #ifdef HAVE_HIP
    // TODO: remove fallback when hipblas provides this routine
    magmablas_ssymm(
        side, uplo, m, n,
        alpha, (magmaFloat_ptr)dA, ldda,
               (magmaFloat_ptr)dB, lddb,
        beta,  dC, lddc, queue );
    #else
    cublasSsymm(
        queue->cublas_handle(),
        cublas_side_const( side ),
        cublas_uplo_const( uplo ),
        int(m), int(n),
        (float*)&alpha, (float*)dA, int(ldda),
                (float*)dB, int(lddb),
        (float*)&beta,  (float*)dC, int(lddc) );
    #endif
}
#endif // COMPLEX


#ifdef COMPLEX
/***************************************************************************//**
    Perform symmetric rank-k update.
        \f$ C = \alpha A A^H + \beta C \f$ (trans == MagmaNoTrans), or \n
        \f$ C = \alpha A^H A + \beta C \f$ (trans == MagmaConjTrans), \n
    where \f$ C \f$ is symmetric.

    @param[in]
    uplo    Whether the upper or lower triangle of C is referenced.

    @param[in]
    trans   Operation to perform on A.

    @param[in]
    n       Number of rows and columns of C. n >= 0.

    @param[in]
    k       Number of columns of A (for MagmaNoTrans)
            or rows of A (for MagmaConjTrans). k >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dA      REAL array on GPU device.
            If trans == MagmaNoTrans, the n-by-k matrix A of dimension (ldda,k), ldda >= max(1,n); \n
            otherwise,                the k-by-n matrix A of dimension (ldda,n), ldda >= max(1,k).

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    beta    Scalar \f$ \beta \f$

    @param[in,out]
    dC      REAL array on GPU device.
            The n-by-n symmetric matrix C of dimension (lddc,n), lddc >= max(1,n).

    @param[in]
    lddc    Leading dimension of dC.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_herk
*******************************************************************************/
extern "C" void
magma_ssyrk(
    magma_uplo_t uplo, magma_trans_t trans,
    magma_int_t n, magma_int_t k,
    float alpha,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    float beta,
    magmaFloat_ptr       dC, magma_int_t lddc,
    magma_queue_t queue )
{
    #ifdef HAVE_HIP
    // TODO: remove fallback when hipblas provides this routine
    magmablas_ssyrk(
        uplo, trans, n, k,
        alpha, (magmaFloat_ptr)dA, ldda,
        beta,  dC, lddc, queue );
    #else
    cublasSsyrk(
        queue->cublas_handle(),
        cublas_uplo_const( uplo ),
        cublas_trans_const( trans ),
        int(n), int(k),
        &alpha, (float*)dA, int(ldda),
        &beta,  (float*)dC, int(lddc) );
    #endif
}
#endif // COMPLEX


#ifdef COMPLEX
/***************************************************************************//**
    Perform symmetric rank-2k update.
        \f$ C = \alpha A B^H + \alpha B A^H \beta C \f$ (trans == MagmaNoTrans), or \n
        \f$ C = \alpha A^H B + \alpha B^H A \beta C \f$ (trans == MagmaConjTrans), \n
    where \f$ C \f$ is symmetric.

    @param[in]
    uplo    Whether the upper or lower triangle of C is referenced.

    @param[in]
    trans   Operation to perform on A and B.

    @param[in]
    n       Number of rows and columns of C. n >= 0.

    @param[in]
    k       Number of columns of A and B (for MagmaNoTrans)
            or rows of A and B (for MagmaConjTrans). k >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dA      REAL array on GPU device.
            If trans == MagmaNoTrans, the n-by-k matrix A of dimension (ldda,k), ldda >= max(1,n); \n
            otherwise,                the k-by-n matrix A of dimension (ldda,n), ldda >= max(1,k).

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    dB      REAL array on GPU device.
            If trans == MagmaNoTrans, the n-by-k matrix B of dimension (lddb,k), lddb >= max(1,n); \n
            otherwise,                the k-by-n matrix B of dimension (lddb,n), lddb >= max(1,k).

    @param[in]
    lddb    Leading dimension of dB.

    @param[in]
    beta    Scalar \f$ \beta \f$

    @param[in,out]
    dC      REAL array on GPU device.
            The n-by-n symmetric matrix C of dimension (lddc,n), lddc >= max(1,n).

    @param[in]
    lddc    Leading dimension of dC.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_her2k
*******************************************************************************/
extern "C" void
magma_ssyr2k(
    magma_uplo_t uplo, magma_trans_t trans,
    magma_int_t n, magma_int_t k,
    float alpha,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magmaFloat_const_ptr dB, magma_int_t lddb,
    float beta,
    magmaFloat_ptr       dC, magma_int_t lddc,
    magma_queue_t queue )
{
    #ifdef HAVE_HIP
    // TODO: remove fallback when hipblas provides this routine
    magmablas_ssyr2k(
        uplo, trans, n, k,
        alpha, (magmaFloat_ptr)dA, ldda,
               (magmaFloat_ptr)dB, lddb,
        beta,  dC, lddc, queue );
    #else
    cublasSsyr2k(
        queue->cublas_handle(),
        cublas_uplo_const( uplo ),
        cublas_trans_const( trans ),
        int(n), int(k),
        (float*)&alpha, (float*)dA, int(ldda),
                (float*)dB, int(lddb),
        &beta,  (float*)dC, int(lddc) );
    #endif
}
#endif // COMPLEX


/***************************************************************************//**
    Perform symmetric matrix-matrix product.
        \f$ C = \alpha A B + \beta C \f$ (side == MagmaLeft), or \n
        \f$ C = \alpha B A + \beta C \f$ (side == MagmaRight),   \n
    where \f$ A \f$ is symmetric.

    @param[in]
    side    Whether A is on the left or right.

    @param[in]
    uplo    Whether the upper or lower triangle of A is referenced.

    @param[in]
    m       Number of rows of C. m >= 0.

    @param[in]
    n       Number of columns of C. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dA      REAL array on GPU device.
            If side == MagmaLeft, the m-by-m symmetric matrix A of dimension (ldda,m), ldda >= max(1,m); \n
            otherwise,            the n-by-n symmetric matrix A of dimension (ldda,n), ldda >= max(1,n).

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    dB      REAL array on GPU device.
            The m-by-n matrix B of dimension (lddb,n), lddb >= max(1,m).

    @param[in]
    lddb    Leading dimension of dB.

    @param[in]
    beta    Scalar \f$ \beta \f$

    @param[in,out]
    dC      REAL array on GPU device.
            The m-by-n matrix C of dimension (lddc,n), lddc >= max(1,m).

    @param[in]
    lddc    Leading dimension of dC.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_symm
*******************************************************************************/
extern "C" void
magma_ssymm(
    magma_side_t side, magma_uplo_t uplo,
    magma_int_t m, magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magmaFloat_const_ptr dB, magma_int_t lddb,
    float beta,
    magmaFloat_ptr       dC, magma_int_t lddc,
    magma_queue_t queue )
{
    #ifdef HAVE_HIP
    // TODO: remove fallback when hipblas provides this routine
    magmablas_ssymm(
        side, uplo, m, n,
        alpha, (magmaFloat_ptr)dA, ldda,
               (magmaFloat_ptr)dB, lddb,
        beta,  dC, lddc, queue );
    #else
    cublasSsymm(
        queue->cublas_handle(),
        cublas_side_const( side ),
        cublas_uplo_const( uplo ),
        int(m), int(n),
        (float*)&alpha, (float*)dA, int(ldda),
                (float*)dB, int(lddb),
        (float*)&beta,  (float*)dC, int(lddc) );
    #endif
}


/***************************************************************************//**
    Perform symmetric rank-k update.
        \f$ C = \alpha A A^T + \beta C \f$ (trans == MagmaNoTrans), or \n
        \f$ C = \alpha A^T A + \beta C \f$ (trans == MagmaTrans),      \n
    where \f$ C \f$ is symmetric.

    @param[in]
    uplo    Whether the upper or lower triangle of C is referenced.

    @param[in]
    trans   Operation to perform on A.

    @param[in]
    n       Number of rows and columns of C. n >= 0.

    @param[in]
    k       Number of columns of A (for MagmaNoTrans)
            or rows of A (for MagmaTrans). k >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dA      REAL array on GPU device.
            If trans == MagmaNoTrans, the n-by-k matrix A of dimension (ldda,k), ldda >= max(1,n); \n
            otherwise,                the k-by-n matrix A of dimension (ldda,n), ldda >= max(1,k).

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    beta    Scalar \f$ \beta \f$

    @param[in,out]
    dC      REAL array on GPU device.
            The n-by-n symmetric matrix C of dimension (lddc,n), lddc >= max(1,n).

    @param[in]
    lddc    Leading dimension of dC.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_syrk
*******************************************************************************/
extern "C" void
magma_ssyrk(
    magma_uplo_t uplo, magma_trans_t trans,
    magma_int_t n, magma_int_t k,
    float alpha,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    float beta,
    magmaFloat_ptr       dC, magma_int_t lddc,
    magma_queue_t queue )
{
    #ifdef HAVE_HIP
    // TODO: remove fallback when hipblas provides this routine
    magmablas_ssyrk(
        uplo, trans, n, k,
        alpha, (magmaFloat_ptr)dA, ldda,
        beta,  dC, lddc, queue );
    #else
    cublasSsyrk(
        queue->cublas_handle(),
        cublas_uplo_const( uplo ),
        cublas_trans_const( trans ),
        int(n), int(k),
        (float*)&alpha, (float*)dA, int(ldda),
        (float*)&beta,  (float*)dC, int(lddc) );
    #endif
}


/***************************************************************************//**
    Perform symmetric rank-2k update.
        \f$ C = \alpha A B^T + \alpha B A^T \beta C \f$ (trans == MagmaNoTrans), or \n
        \f$ C = \alpha A^T B + \alpha B^T A \beta C \f$ (trans == MagmaTrans),      \n
    where \f$ C \f$ is symmetric.

    @param[in]
    uplo    Whether the upper or lower triangle of C is referenced.

    @param[in]
    trans   Operation to perform on A and B.

    @param[in]
    n       Number of rows and columns of C. n >= 0.

    @param[in]
    k       Number of columns of A and B (for MagmaNoTrans)
            or rows of A and B (for MagmaTrans). k >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dA      REAL array on GPU device.
            If trans == MagmaNoTrans, the n-by-k matrix A of dimension (ldda,k), ldda >= max(1,n); \n
            otherwise,                the k-by-n matrix A of dimension (ldda,n), ldda >= max(1,k).

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    dB      REAL array on GPU device.
            If trans == MagmaNoTrans, the n-by-k matrix B of dimension (lddb,k), lddb >= max(1,n); \n
            otherwise,                the k-by-n matrix B of dimension (lddb,n), lddb >= max(1,k).

    @param[in]
    lddb    Leading dimension of dB.

    @param[in]
    beta    Scalar \f$ \beta \f$

    @param[in,out]
    dC      REAL array on GPU device.
            The n-by-n symmetric matrix C of dimension (lddc,n), lddc >= max(1,n).

    @param[in]
    lddc    Leading dimension of dC.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_syr2k
*******************************************************************************/
extern "C" void
magma_ssyr2k(
    magma_uplo_t uplo, magma_trans_t trans,
    magma_int_t n, magma_int_t k,
    float alpha,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magmaFloat_const_ptr dB, magma_int_t lddb,
    float beta,
    magmaFloat_ptr       dC, magma_int_t lddc,
    magma_queue_t queue )
{
    #ifdef HAVE_HIP
    // TODO: remove fallback when hipblas provides this routine
    magmablas_ssyr2k(
        uplo, trans, n, k,
        alpha, (magmaFloat_ptr)dA, ldda,
               (magmaFloat_ptr)dB, lddb,
        beta,  dC, lddc, queue );
    #else
    cublasSsyr2k(
        queue->cublas_handle(),
        cublas_uplo_const( uplo ),
        cublas_trans_const( trans ),
        int(n), int(k),
        (float*)&alpha, (float*)dA, int(ldda),
                (float*)dB, int(lddb),
        (float*)&beta,  (float*)dC, int(lddc) );
    #endif
}


/***************************************************************************//**
    Perform triangular matrix-matrix product.
        \f$ B = \alpha op(A) B \f$ (side == MagmaLeft), or \n
        \f$ B = \alpha B op(A) \f$ (side == MagmaRight),   \n
    where \f$ A \f$ is triangular.

    @param[in]
    side    Whether A is on the left or right.

    @param[in]
    uplo    Whether A is upper or lower triangular.

    @param[in]
    trans   Operation to perform on A.

    @param[in]
    diag    Whether the diagonal of A is assumed to be unit or non-unit.

    @param[in]
    m       Number of rows of B. m >= 0.

    @param[in]
    n       Number of columns of B. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dA      REAL array on GPU device.
            If side == MagmaLeft, the n-by-n triangular matrix A of dimension (ldda,n), ldda >= max(1,n); \n
            otherwise,            the m-by-m triangular matrix A of dimension (ldda,m), ldda >= max(1,m).

    @param[in]
    ldda    Leading dimension of dA.

    @param[in]
    dB      REAL array on GPU device.
            The m-by-n matrix B of dimension (lddb,n), lddb >= max(1,m).

    @param[in]
    lddb    Leading dimension of dB.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_trmm
*******************************************************************************/
extern "C" void
magma_strmm(
    magma_side_t side, magma_uplo_t uplo, magma_trans_t trans, magma_diag_t diag,
    magma_int_t m, magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magmaFloat_ptr       dB, magma_int_t lddb,
    magma_queue_t queue )
{
    #ifdef HAVE_HIP
    // TODO: remove fallback when hipblas provides this routine
    magmablas_strmm(
        side, uplo, trans, diag, m, n,
        alpha, (magmaFloat_ptr)dA, ldda,
               (magmaFloat_ptr)dB, lddb, queue );
    #else
    cublasStrmm(
        queue->cublas_handle(),
        cublas_side_const( side ),
        cublas_uplo_const( uplo ),
        cublas_trans_const( trans ),
        cublas_diag_const( diag ),
        int(m), int(n),
        &alpha, dA, int(ldda),
                dB, int(lddb),
                dB, int(lddb) );  /* C same as B; less efficient */
    #endif
}


/***************************************************************************//**
    Solve triangular matrix-matrix system (multiple right-hand sides).
        \f$ op(A) X = \alpha B \f$ (side == MagmaLeft), or \n
        \f$ X op(A) = \alpha B \f$ (side == MagmaRight),   \n
    where \f$ A \f$ is triangular.

    @param[in]
    side    Whether A is on the left or right.

    @param[in]
    uplo    Whether A is upper or lower triangular.

    @param[in]
    trans   Operation to perform on A.

    @param[in]
    diag    Whether the diagonal of A is assumed to be unit or non-unit.

    @param[in]
    m       Number of rows of B. m >= 0.

    @param[in]
    n       Number of columns of B. n >= 0.

    @param[in]
    alpha   Scalar \f$ \alpha \f$

    @param[in]
    dA      REAL array on GPU device.
            If side == MagmaLeft, the m-by-m triangular matrix A of dimension (ldda,m), ldda >= max(1,m); \n
            otherwise,            the n-by-n triangular matrix A of dimension (ldda,n), ldda >= max(1,n).

    @param[in]
    ldda    Leading dimension of dA.

    @param[in,out]
    dB      REAL array on GPU device.
            On entry, m-by-n matrix B of dimension (lddb,n), lddb >= max(1,m).
            On exit, overwritten with the solution matrix X.

    @param[in]
    lddb    Leading dimension of dB.

    @param[in]
    queue   magma_queue_t
            Queue to execute in.

    @ingroup magma_trsm
*******************************************************************************/
extern "C" void
magma_strsm(
    magma_side_t side, magma_uplo_t uplo, magma_trans_t trans, magma_diag_t diag,
    magma_int_t m, magma_int_t n,
    float alpha,
    magmaFloat_const_ptr dA, magma_int_t ldda,
    magmaFloat_ptr       dB, magma_int_t lddb,
    magma_queue_t queue )
{
    cublasStrsm(
        queue->cublas_handle(),
        cublas_side_const( side ),
        cublas_uplo_const( uplo ),
        cublas_trans_const( trans ),
        cublas_diag_const( diag ),
        int(m), int(n),
        (float*)&alpha, (float*)dA, int(ldda),
                (float*)dB, int(lddb) );
}

//#endif // HAVE_CUBLAS

#undef REAL
