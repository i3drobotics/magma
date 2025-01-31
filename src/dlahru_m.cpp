/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @generated from src/zlahru_m.cpp, normal z -> d, Sat Mar 27 20:31:04 2021
       @author Mark Gates
*/
#include "magma_internal.h"

/***************************************************************************//**
    Purpose
    -------
    DLAHRU is an auxiliary MAGMA routine that is used in DGEHRD to update
    the trailing sub-matrices after the reductions of the corresponding
    panels.
    See further details below.

    Arguments
    ---------
    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in]
    ihi     INTEGER
            Last row to update. Same as IHI in dgehrd.

    @param[in]
    k       INTEGER
            Number of rows of the matrix Am (see details below)

    @param[in]
    nb      INTEGER
            Block size

    @param[out]
    A       DOUBLE PRECISION array, dimension (LDA,N-K)
            On entry, the N-by-(N-K) general matrix to be updated. The
            computation is done on the GPU. After Am is updated on the GPU
            only Am(1:NB) is transferred to the CPU - to update the
            corresponding Am matrix. See Further Details below.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[in,out]
    data    Structure with pointers to dA, dT, dV, dW, dY
            which are distributed across multiple GPUs.

    Further Details
    ---------------
    This implementation follows the algorithm and notations described in:

    S. Tomov and J. Dongarra, "Accelerating the reduction to upper Hessenberg
    form through hybrid GPU-based computing," University of Tennessee Computer
    Science Technical Report, UT-CS-09-642 (also LAPACK Working Note 219),
    May 24, 2009.

    The difference is that here Am is computed on the GPU.
    M is renamed Am, G is renamed Ag.

    @ingroup magma_lahru
*******************************************************************************/
extern "C" magma_int_t
magma_dlahru_m(
    magma_int_t n, magma_int_t ihi, magma_int_t k, magma_int_t nb,
    double *A, magma_int_t lda,
    struct dgehrd_data *data )
{
    #define dA(  dev, i, j ) (data->dA [dev] + (i) + (j)*ldda)
    #define dTi( dev       ) (data->dTi[dev])
    #define dV(  dev, i, j ) (data->dV [dev] + (i) + (j)*ldv )
    #define dVd( dev, i, j ) (data->dVd[dev] + (i) + (j)*ldvd)
    #define dW(  dev, i, j ) (data->dW [dev] + (i) + (j)*ldda)
    #define dY(  dev, i, j ) (data->dY [dev] + (i) + (j)*ldda)
    
    double c_zero    = MAGMA_D_ZERO;
    double c_one     = MAGMA_D_ONE;
    double c_neg_one = MAGMA_D_NEG_ONE;

    magma_int_t ngpu = data->ngpu;
    magma_int_t ldda = data->ldda;
    magma_int_t ldv  = data->ldv;
    magma_int_t ldvd = data->ldvd;
    
    magma_int_t dev;
    magma_int_t dk, dkhi, dknb, dn;
    
    magma_int_t info = 0;
    if (n < 0) {
        info = -1;
    } else if (ihi < 0 || ihi > n) {
        info = -2;
    } else if (k < 0 || k > n) {
        info = -3;
    } else if (nb < 1 || nb > n) {
        info = -4;
    } else if (lda < max(1,n)) {
        info = -6;
    }
    if (info != 0) {
        magma_xerbla( __func__, -(info) );
        return info;
    }
    
    magma_device_t orig_dev;
    magma_getdevice( &orig_dev );
    
    for( dev = 0; dev < ngpu; ++dev ) {
        magma_setdevice( dev );
        
        // convert global indices (k) to local indices (dk)
        magma_indices_1D_bcyclic( nb, ngpu, dev, k,    ihi, &dk,   &dkhi );
        magma_indices_1D_bcyclic( nb, ngpu, dev, k+nb, n,   &dknb, &dn   );
        
        // -----
        // on right, A := A Q = A - A V T V'
        // Update Am = Am - Am V T Vd' = Am - Ym Wd', with Wd = Vd T'
        // Wd = Vd T' = V(k:ihi-1, 0:nb-1) * T(0:nb-1, 0:nb-1)'
        // Vd and Wd are the portions corresponding to the block cyclic dkstribution
        magma_dgemm( MagmaNoTrans, MagmaConjTrans, dkhi-dk, nb, nb,
                     c_one,  dVd(dev, dk, 0), ldvd,
                             dTi(dev),        nb,
                     c_zero, dW (dev, dk, 0), ldda, data->queues[dev] );
        
        // Am = Am - Ym Wd' = A(0:k-1, k:ihi-1) - Ym(0:k-1, 0:nb-1) * W(k:ihi-1, 0:nb-1)'
        magma_dgemm( MagmaNoTrans, MagmaConjTrans, k, dkhi-dk, nb,
                     c_neg_one, dY(dev, 0,  0),  ldda,
                                dW(dev, dk, 0),  ldda,
                     c_one,     dA(dev, 0,  dk), ldda, data->queues[dev] );

        // -----
        // on right, A := A Q = A - A V T V'
        // Update Ag = Ag - Ag V T V' = Ag - Yg Wd'
        // Ag = Ag - Yg Wd' = A(k:ihi-1, nb:ihi-k-1) - Y(k:ihi-1, 0:nb-1) * W(k+nb:ihi-1, 0:nb-1)'
        magma_dgemm( MagmaNoTrans, MagmaConjTrans, ihi-k, dkhi-dknb, nb,
                     c_neg_one, dY(dev, k,    0),    ldda,
                                dW(dev, dknb, 0),    ldda,
                     c_one,     dA(dev, k,    dknb), ldda, data->queues[dev] );
        
        // -----
        // on left, A := Q' A = A - V T' V' A
        // Ag2 = Ag2 - V T' V' Ag2 = W Yg, with W = V T' and Yg = V' Ag2
        // Note that Ag is A(k:ihi, nb+1:ihi-k)
        // while    Ag2 is A(k:ihi, nb+1: n -k)
        
        // here V and W are the whole matrices, not just block cyclic portion
        // W = V T' = V(k:ihi-1, 0:nb-1) * T(0:nb-1, 0:nb-1)'
        // TODO would it be cheaper to compute the whole matrix and
        // copy the block cyclic portions to another workspace?
        magma_dgemm( MagmaNoTrans, MagmaConjTrans, ihi-k, nb, nb,
                     c_one,  dV (dev, k, 0), ldv,
                             dTi(dev),       nb,
                     c_zero, dW (dev, k, 0), ldda, data->queues[dev] );
        
        // Z = V(k:ihi-1, 0:nb-1)' * A(k:ihi-1, nb:n-k-1);  Z is stored over Y
        magma_dgemm( MagmaConjTrans, MagmaNoTrans, nb, dn-dknb, ihi-k,
                     c_one,  dV(dev, k, 0),    ldv,
                             dA(dev, k, dknb), ldda,
                     c_zero, dY(dev, 0, 0),    nb, data->queues[dev] );
        
        // Ag2 = Ag2 - W Z = A(k:ihi-1, k+nb:n-1) - W(k+nb:n-1, 0:nb-1) * Z(0:nb-1, k+nb:n-1)
        magma_dgemm( MagmaNoTrans, MagmaNoTrans, ihi-k, dn-dknb, nb,
                     c_neg_one, dW(dev, k, 0),    ldda,
                                dY(dev, 0, 0),    nb,
                     c_one,     dA(dev, k, dknb), ldda, data->queues[dev] );
    }
    
    magma_setdevice( orig_dev );
    
    return info;
}
