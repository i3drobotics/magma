/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/blas/zmergebicgstab3.cu, normal z -> s, Sat Mar 27 20:32:27 2021
       @author Hartwig Anzt

*/
#include "magmasparse_internal.h"

#define BLOCK_SIZE 512

#define PRECISION_s


// These routines merge multiple kernels from smergebicgstab into one
// The difference to smergedbicgstab2 is that the SpMV is not merged into the
// kernes. This results in higher flexibility at the price of lower performance.

/* -------------------------------------------------------------------------- */

__global__ void
magma_sbicgmerge1_kernel(  
    int n, 
    float * skp,
    float * v, 
    float * r, 
    float * p )
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    float beta=skp[1];
    float omega=skp[2];
    if ( i<n ) {
        p[i] =  r[i] + beta * ( p[i] - omega * v[i] );
    }
}

/**
    Purpose
    -------

    Mergels multiple operations into one kernel:

    p = beta*p
    p = p-omega*beta*v
    p = p+r
    
    -> p = r + beta * ( p - omega * v ) 

    Arguments
    ---------

    @param[in]
    n           int
                dimension n

    @param[in]
    skp         magmaFloat_ptr
                set of scalar parameters

    @param[in]
    v           magmaFloat_ptr
                input vector v

    @param[in]
    r           magmaFloat_ptr
                input vector r

    @param[in,out]
    p           magmaFloat_ptr 
                input/output vector p

    @param[in]
    queue       magma_queue_t
                queue to execute in.

    @ingroup magmasparse_sgegpuk
    ********************************************************************/

extern "C" magma_int_t
magma_sbicgmerge1(  
    magma_int_t n, 
    magmaFloat_ptr skp,
    magmaFloat_ptr v, 
    magmaFloat_ptr r, 
    magmaFloat_ptr p,
    magma_queue_t queue )
{
    dim3 Bs( BLOCK_SIZE );
    dim3 Gs( magma_ceildiv( n, BLOCK_SIZE ) );
    magma_sbicgmerge1_kernel<<< Gs, Bs, 0, queue->cuda_stream() >>>( n, skp, v, r, p );

    return MAGMA_SUCCESS;
}

/* -------------------------------------------------------------------------- */

__global__ void
magma_sbicgmerge2_kernel(  
    int n, 
    float * skp, 
    float * r,
    float * v, 
    float * s )
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    float alpha=skp[0];
    if ( i < n ) {
        s[i] =  r[i] - alpha * v[i];
    }
}

/**
    Purpose
    -------

    Mergels multiple operations into one kernel:

    s=r
    s=s-alpha*v
        
    -> s = r - alpha * v

    Arguments
    ---------

    @param[in]
    n           int
                dimension n

    @param[in]
    skp         magmaFloat_ptr 
                set of scalar parameters

    @param[in]
    r           magmaFloat_ptr 
                input vector r

    @param[in]
    v           magmaFloat_ptr 
                input vector v

    @param[out]
    s           magmaFloat_ptr 
                output vector s

    @param[in]
    queue       magma_queue_t
                queue to execute in.

    @ingroup magmasparse_sgegpuk
    ********************************************************************/

extern "C" magma_int_t
magma_sbicgmerge2(  
    magma_int_t n, 
    magmaFloat_ptr skp, 
    magmaFloat_ptr r,
    magmaFloat_ptr v, 
    magmaFloat_ptr s,
    magma_queue_t queue )
{
    dim3 Bs( BLOCK_SIZE );
    dim3 Gs( magma_ceildiv( n, BLOCK_SIZE ) );

    magma_sbicgmerge2_kernel<<< Gs, Bs, 0, queue->cuda_stream() >>>( n, skp, r, v, s );

    return MAGMA_SUCCESS;
}

/* -------------------------------------------------------------------------- */

__global__ void
magma_sbicgmerge3_kernel(  
    int n, 
    float * skp, 
    float * p,
    float * se,
    float * t,
    float * x, 
    float * r
    )
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    float alpha=skp[0];
    float omega=skp[2];
    if ( i<n ) {
        float s;
        s = se[i];
        x[i] = x[i] + alpha * p[i] + omega * s;
        r[i] = s - omega * t[i];
    }
}

/**
    Purpose
    -------

    Mergels multiple operations into one kernel:

    x=x+alpha*p
    x=x+omega*s
    r=s
    r=r-omega*t
        
    -> x = x + alpha * p + omega * s
    -> r = s - omega * t

    Arguments
    ---------

    @param[in]
    n           int
                dimension n

    @param[in]
    skp         magmaFloat_ptr 
                set of scalar parameters

    @param[in]
    p           magmaFloat_ptr 
                input p

    @param[in]
    s           magmaFloat_ptr 
                input s

    @param[in]
    t           magmaFloat_ptr 
                input t

    @param[in,out]
    x           magmaFloat_ptr 
                input/output x

    @param[in,out]
    r           magmaFloat_ptr 
                input/output r

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_sgegpuk
    ********************************************************************/

extern "C" magma_int_t
magma_sbicgmerge3(  
    magma_int_t n, 
    magmaFloat_ptr skp,
    magmaFloat_ptr p,
    magmaFloat_ptr s,
    magmaFloat_ptr t,
    magmaFloat_ptr x, 
    magmaFloat_ptr r,
    magma_queue_t queue )
{
    dim3 Bs( BLOCK_SIZE );
    dim3 Gs( magma_ceildiv( n, BLOCK_SIZE ) );
    magma_sbicgmerge3_kernel<<< Gs, Bs, 0, queue->cuda_stream() >>>( n, skp, p, s, t, x, r );

    return MAGMA_SUCCESS;
}

/* -------------------------------------------------------------------------- */

__global__ void
magma_sbicgmerge4_kernel_1(  
    float * skp )
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;

    if ( i==0 ) {
        float tmp = skp[0];
        skp[0] = skp[4]/tmp;
    }
}

__global__ void
magma_sbicgmerge4_kernel_2(  
    float * skp )
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;

    if ( i==0 ) {
        skp[2] = skp[6]/skp[7];
        skp[3] = skp[4];
    }
}

__global__ void
magma_sbicgmerge4_kernel_3(  
    float * skp )
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;

    if ( i==0 ) {
        float tmp1 = skp[4]/skp[3];
        float tmp2 = skp[0] / skp[2];
        skp[1] =  tmp1*tmp2;
        //skp[1] =  skp[4]/skp[3] * skp[0] / skp[2];
    }
}

/**
    Purpose
    -------

    Performs some parameter operations for the BiCGSTAB with scalars on GPU.

    Arguments
    ---------

    @param[in]
    type        int
                kernel type

    @param[in,out]
    skp         magmaFloat_ptr 
                vector with parameters

    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_sgegpuk
    ********************************************************************/

extern "C" magma_int_t
magma_sbicgmerge4(  
    magma_int_t type, 
    magmaFloat_ptr skp,
    magma_queue_t queue )
{
    dim3 Bs( 1 );
    dim3 Gs( 1 );
    if ( type == 1 )
        magma_sbicgmerge4_kernel_1<<< Gs, Bs, 0, queue->cuda_stream() >>>( skp );
    else if ( type == 2 )
        magma_sbicgmerge4_kernel_2<<< Gs, Bs, 0, queue->cuda_stream() >>>( skp );
    else if ( type == 3 )
        magma_sbicgmerge4_kernel_3<<< Gs, Bs, 0, queue->cuda_stream() >>>( skp );
    else
        printf("error: no kernel called\n");

    return MAGMA_SUCCESS;
}
