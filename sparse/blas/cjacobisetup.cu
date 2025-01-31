/*
    -- MAGMA (version 1.1) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from sparse/blas/zjacobisetup.cu, normal z -> c, Sat Mar 27 20:32:25 2021
       @author Hartwig Anzt

*/
#include "magmasparse_internal.h"

#define BLOCK_SIZE 512


#define PRECISION_c

__global__ void 
cvjacobisetup_gpu(  int num_rows, 
                    int num_vecs,
                    magmaFloatComplex *b, 
                    magmaFloatComplex *d, 
                    magmaFloatComplex *c,
                    magmaFloatComplex *x)
{
    int row = blockDim.x * blockIdx.x + threadIdx.x;

    if(row < num_rows ){
        for( int i=0; i<num_vecs; i++ ){
            c[row+i*num_rows] = b[row+i*num_rows] / d[row];
            x[row+i*num_rows] = c[row+i*num_rows];
        }
    }
}


/**
    Purpose
    -------

    Prepares the Jacobi Iteration according to
       x^(k+1) = D^(-1) * b - D^(-1) * (L+U) * x^k
       x^(k+1) =      c     -       M        * x^k.

    Returns the vector c. It calls a GPU kernel

    Arguments
    ---------

    @param[in]
    num_rows    magma_int_t
                number of rows
                
    @param[in]
    b           magma_c_matrix
                RHS b

    @param[in]
    d           magma_c_matrix
                vector with diagonal entries

    @param[out]
    c           magma_c_matrix*
                c = D^(-1) * b

    @param[out]
    x           magma_c_matrix*
                iteration vector
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_cgegpuk
    ********************************************************************/

extern "C" magma_int_t
magma_cjacobisetup_vector_gpu(
    magma_int_t num_rows, 
    magma_c_matrix b, 
    magma_c_matrix d, 
    magma_c_matrix c,
    magma_c_matrix *x,
    magma_queue_t queue )
{
    dim3 grid( magma_ceildiv( num_rows, BLOCK_SIZE ) );
    int num_vecs = b.num_rows / num_rows;
    magma_int_t threads = BLOCK_SIZE;
    cvjacobisetup_gpu<<< grid, threads, 0, queue->cuda_stream()>>>
                ( num_rows, num_vecs, b.dval, d.dval, c.dval, x->val );

    return MAGMA_SUCCESS;
}


__global__ void 
cjacobidiagscal_kernel(  int num_rows,
                         int num_vecs, 
                    magmaFloatComplex *b, 
                    magmaFloatComplex *d, 
                    magmaFloatComplex *c)
{
    int row = blockDim.x * blockIdx.x + threadIdx.x;

    if(row < num_rows ){
        for( int i=0; i<num_vecs; i++)
            c[row+i*num_rows] = b[row+i*num_rows] * d[row];
    }
}


/**
    Purpose
    -------

    Prepares the Jacobi Iteration according to
       x^(k+1) = D^(-1) * b - D^(-1) * (L+U) * x^k
       x^(k+1) =      c     -       M        * x^k.

    Returns the vector c. It calls a GPU kernel

    Arguments
    ---------

    @param[in]
    num_rows    magma_int_t
                number of rows
                
    @param[in]
    b           magma_c_matrix
                RHS b

    @param[in]
    d           magma_c_matrix
                vector with diagonal entries

    @param[out]
    c           magma_c_matrix*
                c = D^(-1) * b
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_c
    ********************************************************************/

extern "C" magma_int_t
magma_cjacobi_diagscal(
    magma_int_t num_rows, 
    magma_c_matrix d, 
    magma_c_matrix b, 
    magma_c_matrix *c,
    magma_queue_t queue )
{
    dim3 grid( magma_ceildiv( num_rows, 512 ));
    int num_vecs = b.num_rows*b.num_cols/num_rows;
    magma_int_t threads = 512;
    cjacobidiagscal_kernel<<< grid, threads, 0, queue->cuda_stream()>>>( num_rows, num_vecs, b.dval, d.dval, c->val );

    return MAGMA_SUCCESS;
}


__global__ void 
cjacobiupdate_kernel(  int num_rows,
                       int num_cols, 
                    magmaFloatComplex *t, 
                    magmaFloatComplex *b, 
                    magmaFloatComplex *d, 
                    magmaFloatComplex *x)
{
    int row = blockDim.x * blockIdx.x + threadIdx.x;

    if(row < num_rows ){
        for( int i=0; i<num_cols; i++)
            x[row+i*num_rows] += (b[row+i*num_rows]-t[row+i*num_rows]) * d[row];
    }
}


/**
    Purpose
    -------

    Updates the iteration vector x for the Jacobi iteration
    according to
        x=x+d.*(b-t)
    where d is the diagonal of the system matrix A and t=Ax.

    Arguments
    ---------
                
    @param[in]
    t           magma_c_matrix
                t = A*x
                
    @param[in]
    b           magma_c_matrix
                RHS b
                
    @param[in]
    d           magma_c_matrix
                vector with diagonal entries

    @param[out]
    x           magma_c_matrix*
                iteration vector
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_c
    ********************************************************************/

extern "C" magma_int_t
magma_cjacobiupdate(
    magma_c_matrix t, 
    magma_c_matrix b, 
    magma_c_matrix d, 
    magma_c_matrix *x,
    magma_queue_t queue )
{
    dim3 grid( magma_ceildiv( t.num_rows, BLOCK_SIZE ));
    magma_int_t threads = BLOCK_SIZE;
    cjacobiupdate_kernel<<< grid, threads, 0, queue->cuda_stream()>>>( t.num_rows, t.num_cols, t.dval, b.dval, d.dval, x->dval );

    return MAGMA_SUCCESS;
}


__global__ void 
cjacobispmvupdate_kernel(  
    int num_rows,
    int num_cols, 
    magmaFloatComplex * dval, 
    magma_index_t * drowptr, 
    magma_index_t * dcolind,
    magmaFloatComplex *t, 
    magmaFloatComplex *b, 
    magmaFloatComplex *d, 
    magmaFloatComplex *x )
{
    int row = blockDim.x * blockIdx.x + threadIdx.x;
    int j;

    if(row<num_rows){
        magmaFloatComplex dot = MAGMA_C_ZERO;
        int start = drowptr[ row ];
        int end = drowptr[ row+1 ];
        for( int i=0; i<num_cols; i++){
            for( j=start; j<end; j++){
                dot += dval[ j ] * x[ dcolind[j]+i*num_rows ];
            }
            x[row+i*num_rows] += (b[row+i*num_rows]-dot) * d[row];
        }
    }
}


/**
    Purpose
    -------

    Updates the iteration vector x for the Jacobi iteration
    according to
        x=x+d.*(b-Ax)


    Arguments
    ---------

    @param[in]
    maxiter     magma_int_t
                number of Jacobi iterations   
                
    @param[in]
    A           magma_c_matrix
                system matrix
                
    @param[in]
    t           magma_c_matrix
                workspace
                
    @param[in]
    b           magma_c_matrix
                RHS b
                
    @param[in]
    d           magma_c_matrix
                vector with diagonal entries

    @param[out]
    x           magma_c_matrix*
                iteration vector
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_c
    ********************************************************************/

extern "C" magma_int_t
magma_cjacobispmvupdate(
    magma_int_t maxiter,
    magma_c_matrix A,
    magma_c_matrix t, 
    magma_c_matrix b, 
    magma_c_matrix d, 
    magma_c_matrix *x,
    magma_queue_t queue )
{
    // local variables
    //magmaFloatComplex c_zero = MAGMA_C_ZERO;
    //magmaFloatComplex c_one = MAGMA_C_ONE;

    dim3 grid( magma_ceildiv( t.num_rows, BLOCK_SIZE ));
    magma_int_t threads = BLOCK_SIZE;

    for( magma_int_t i=0; i<maxiter; i++ ) {
        // distinct routines imply synchronization
        // magma_c_spmv( c_one, A, *x, c_zero, t, queue );                // t =  A * x
        // cjacobiupdate_kernel<<< grid, threads, 0, queue->cuda_stream()>>>( t.num_rows, t.num_cols, t.dval, b.dval, d.dval, x->dval );
        // merged in one implies asynchronous update
        cjacobispmvupdate_kernel<<< grid, threads, 0, queue->cuda_stream()>>>
            ( t.num_rows, t.num_cols, A.dval, A.drow, A.dcol, t.dval, b.dval, d.dval, x->dval );
    }

    return MAGMA_SUCCESS;
}


__global__ void 
cjacobispmvupdate_bw_kernel(  
    int num_rows,
    int num_cols, 
    magmaFloatComplex * dval, 
    magma_index_t * drowptr, 
    magma_index_t * dcolind,
    magmaFloatComplex *t, 
    magmaFloatComplex *b, 
    magmaFloatComplex *d, 
    magmaFloatComplex *x )
{
    int row_tmp = blockDim.x * blockIdx.x + threadIdx.x;
    int row = num_rows-1 - row_tmp;
    int j;

    if( row>-1 ){
        magmaFloatComplex dot = MAGMA_C_ZERO;
        int start = drowptr[ row ];
        int end = drowptr[ row+1 ];
        for( int i=0; i<num_cols; i++){
            for( j=start; j<end; j++){
                dot += dval[ j ] * x[ dcolind[j]+i*num_rows ];
            }
            x[row+i*num_rows] += (b[row+i*num_rows]-dot) * d[row];
        }
    }
}


/**
    Purpose
    -------

    Updates the iteration vector x for the Jacobi iteration
    according to
        x=x+d.*(b-Ax)
    This kernel processes the thread blocks in reversed order.

    Arguments
    ---------

    @param[in]
    maxiter     magma_int_t
                number of Jacobi iterations   
                
    @param[in]
    A           magma_c_matrix
                system matrix
                
    @param[in]
    t           magma_c_matrix
                workspace
                
    @param[in]
    b           magma_c_matrix
                RHS b
                
    @param[in]
    d           magma_c_matrix
                vector with diagonal entries

    @param[out]
    x           magma_c_matrix*
                iteration vector
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_c
    ********************************************************************/

extern "C" magma_int_t
magma_cjacobispmvupdate_bw(
    magma_int_t maxiter,
    magma_c_matrix A,
    magma_c_matrix t, 
    magma_c_matrix b, 
    magma_c_matrix d, 
    magma_c_matrix *x,
    magma_queue_t queue )
{
    // local variables
    //magmaFloatComplex c_zero = MAGMA_C_ZERO;
    //magmaFloatComplex c_one = MAGMA_C_ONE;

    dim3 grid( magma_ceildiv( t.num_rows, BLOCK_SIZE ));
    magma_int_t threads = BLOCK_SIZE;

    for( magma_int_t i=0; i<maxiter; i++ ) {
        // distinct routines imply synchronization
        // magma_c_spmv( c_one, A, *x, c_zero, t, queue );                // t =  A * x
        // cjacobiupdate_kernel<<< grid, threads, 0, queue->cuda_stream()>>>( t.num_rows, t.num_cols, t.dval, b.dval, d.dval, x->dval );
        // merged in one implies asynchronous update
        cjacobispmvupdate_bw_kernel<<< grid, threads, 0, queue->cuda_stream()>>>
            ( t.num_rows, t.num_cols, A.dval, A.drow, A.dcol, t.dval, b.dval, d.dval, x->dval );
    }

    return MAGMA_SUCCESS;
}


__global__ void 
cjacobispmvupdateselect_kernel(  
    int num_rows,
    int num_cols, 
    int num_updates, 
    magma_index_t * indices, 
    magmaFloatComplex * dval, 
    magma_index_t * drowptr, 
    magma_index_t * dcolind,
    magmaFloatComplex *t, 
    magmaFloatComplex *b, 
    magmaFloatComplex *d, 
    magmaFloatComplex *x,
    magmaFloatComplex *y )
{
    int idx = blockDim.x * blockIdx.x + threadIdx.x;
    int j;

    if(  idx<num_updates){
        int row = indices[ idx ];
        printf(" ");    
        //if( row < num_rows ){
        magmaFloatComplex dot = MAGMA_C_ZERO;
        int start = drowptr[ row ];
        int end = drowptr[ row+1 ];
        for( int i=0; i<num_cols; i++){
            for( j=start; j<end; j++){
                dot += dval[ j ] * x[ dcolind[j]+i*num_rows ];
            }
            x[row+i*num_rows] = x[row+i*num_rows] + (b[row+i*num_rows]-dot) * d[row];
            
            //magmaFloatComplex add = (b[row+i*num_rows]-dot) * d[row];
            //#if defined(PRECISION_s) //|| defined(PRECISION_d)
            //    atomicAdd( x + row + i*num_rows, add );  
            //#endif
            // ( unsigned int* address, unsigned int val);
        //}
        }
    }
}


/**
    Purpose
    -------

    Updates the iteration vector x for the Jacobi iteration
    according to
        x=x+d.*(b-Ax)
        
    This kernel allows for overlapping domains: the indices-array contains
    the locations that are updated. Locations may be repeated to simulate
    overlapping domains.


    Arguments
    ---------

    @param[in]
    maxiter     magma_int_t
                number of Jacobi iterations
                
    @param[in]
    num_updates magma_int_t
                number of updates - length of the indices array
                    
    @param[in]
    indices     magma_index_t*
                indices, which entries of x to update
                
    @param[in]
    A           magma_c_matrix
                system matrix
                
    @param[in]
    t           magma_c_matrix
                workspace
                
    @param[in]
    b           magma_c_matrix
                RHS b
                
    @param[in]
    d           magma_c_matrix
                vector with diagonal entries
   
    @param[in]
    tmp         magma_c_matrix
                workspace

    @param[out]
    x           magma_c_matrix*
                iteration vector
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_c
    ********************************************************************/

extern "C" magma_int_t
magma_cjacobispmvupdateselect(
    magma_int_t maxiter,
    magma_int_t num_updates,
    magma_index_t *indices,
    magma_c_matrix A,
    magma_c_matrix t, 
    magma_c_matrix b, 
    magma_c_matrix d, 
    magma_c_matrix tmp, 
    magma_c_matrix *x,
    magma_queue_t queue )
{
    // local variables
    //magmaFloatComplex c_zero = MAGMA_C_ZERO
    //magmaFloatComplex c_one = MAGMA_C_ONE;
    
    //magma_c_matrix swp;

    dim3 grid( magma_ceildiv( num_updates, BLOCK_SIZE ));
    magma_int_t threads = BLOCK_SIZE;
    printf("num updates:%d %d %d\n", int(num_updates), int(threads), int(grid.x) );

    for( magma_int_t i=0; i<maxiter; i++ ) {
        cjacobispmvupdateselect_kernel<<< grid, threads, 0, queue->cuda_stream()>>>
            ( t.num_rows, t.num_cols, num_updates, indices, A.dval, A.drow, A.dcol, t.dval, b.dval, d.dval, x->dval, tmp.dval );
        //swp.dval = x->dval;
        //x->dval = tmp.dval;
        //tmp.dval = swp.dval;
    }
    
    return MAGMA_SUCCESS;
}


__global__ void 
cftjacobicontractions_kernel(
    int num_rows,
    magmaFloatComplex * xkm2val, 
    magmaFloatComplex * xkm1val, 
    magmaFloatComplex * xkval, 
    magmaFloatComplex * zval,
    magmaFloatComplex * cval )
{
    int idx = blockDim.x * blockIdx.x + threadIdx.x;

    if(  idx<num_rows ){
        zval[idx] = MAGMA_C_MAKE( MAGMA_C_ABS( xkm1val[idx] - xkval[idx] ), 0.0);
        cval[ idx ] = MAGMA_C_MAKE(
            MAGMA_C_ABS( xkm2val[idx] - xkm1val[idx] ) 
                / MAGMA_C_ABS( xkm1val[idx] - xkval[idx] )
                                        ,0.0 );
    }
}


/**
    Purpose
    -------

    Computes the contraction coefficients c_i:
    
    c_i = z_i^{k-1} / z_i^{k} 
        
        = | x_i^{k-1} - x_i^{k-2} | / |  x_i^{k} - x_i^{k-1} |

    Arguments
    ---------

    @param[in]
    xkm2        magma_c_matrix
                vector x^{k-2}
                
    @param[in]
    xkm1        magma_c_matrix
                vector x^{k-2}
                
    @param[in]
    xk          magma_c_matrix
                vector x^{k-2}
   
    @param[out]
    z           magma_c_matrix*
                ratio
                
    @param[out]
    c           magma_c_matrix*
                contraction coefficients
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_c
    ********************************************************************/

extern "C" magma_int_t
magma_cftjacobicontractions(
    magma_c_matrix xkm2,
    magma_c_matrix xkm1, 
    magma_c_matrix xk, 
    magma_c_matrix *z,
    magma_c_matrix *c,
    magma_queue_t queue )
{
    dim3 grid( magma_ceildiv( xk.num_rows, BLOCK_SIZE ));
    magma_int_t threads = BLOCK_SIZE;

    cftjacobicontractions_kernel<<< grid, threads, 0, queue->cuda_stream()>>>
            ( xkm2.num_rows, xkm2.dval, xkm1.dval, xk.dval, z->dval, c->dval );
    
    return MAGMA_SUCCESS;
}


__global__ void 
cftjacobiupdatecheck_kernel(
    int num_rows,
    float delta,
    magmaFloatComplex * xold, 
    magmaFloatComplex * xnew, 
    magmaFloatComplex * zprev,
    magmaFloatComplex * cval, 
    magma_int_t *flag_t,
    magma_int_t *flag_fp )
{
    int idx = blockDim.x * blockIdx.x + threadIdx.x;

    if(  idx<num_rows ){
        float t1 = delta * MAGMA_C_ABS(cval[idx]);
        float  vkv = 1.0;
        for( magma_int_t i=0; i<min( flag_fp[idx], 100 ); i++){
            vkv = vkv*2;
        }
        magmaFloatComplex xold_l = xold[idx];
        magmaFloatComplex xnew_l = xnew[idx];
        magmaFloatComplex znew = MAGMA_C_MAKE(
                        max( MAGMA_C_ABS( xold_l - xnew_l), 1e-15), 0.0 );
                        
        magmaFloatComplex znr = zprev[idx] / znew; 
        float t2 = MAGMA_C_ABS( znr - cval[idx] );
        
        //% evaluate fp-cond
        magma_int_t fpcond = 0;
        if( MAGMA_C_ABS(znr)>vkv ){
            fpcond = 1;
        }
        
        // % combine t-cond and fp-cond + flag_t == 1
        magma_int_t cond = 0;
        if( t2<t1 || (flag_t[idx]>0 && fpcond > 0 ) ){
            cond = 1;
        }
        flag_fp[idx] = flag_fp[idx]+1;
        if( fpcond>0 ){
            flag_fp[idx] = 0;
        }
        if( cond > 0 ){
            flag_t[idx] = 0;
            zprev[idx] = znew;
            xold[idx] = xnew_l;
        } else {
            flag_t[idx] = 1;
            xnew[idx] = xold_l;
        }
    }
}


/**
    Purpose
    -------

    Checks the Jacobi updates accorting to the condition in the ScaLA'15 paper.

    Arguments
    ---------
    
    @param[in]
    delta       float
                threshold

    @param[in,out]
    xold        magma_c_matrix*
                vector xold
                
    @param[in,out]
    xnew        magma_c_matrix*
                vector xnew
                
    @param[in,out]
    zprev       magma_c_matrix*
                vector z = | x_k-1 - x_k |
   
    @param[in]
    c           magma_c_matrix
                contraction coefficients
                
    @param[in,out]
    flag_t      magma_int_t
                threshold condition
                
    @param[in,out]
    flag_fp     magma_int_t
                false positive condition
                
    @param[in]
    queue       magma_queue_t
                Queue to execute in.

    @ingroup magmasparse_c
    ********************************************************************/

extern "C" magma_int_t
magma_cftjacobiupdatecheck(
    float delta,
    magma_c_matrix *xold,
    magma_c_matrix *xnew, 
    magma_c_matrix *zprev, 
    magma_c_matrix c,
    magma_int_t *flag_t,
    magma_int_t *flag_fp,
    magma_queue_t queue )
{
    dim3 grid( magma_ceildiv( xnew->num_rows, BLOCK_SIZE ));
    magma_int_t threads = BLOCK_SIZE;

    cftjacobiupdatecheck_kernel<<< grid, threads, 0, queue->cuda_stream()>>>
            ( xold->num_rows, delta, xold->dval, xnew->dval, zprev->dval, c.dval, 
                flag_t, flag_fp );
    
    return MAGMA_SUCCESS;
}
