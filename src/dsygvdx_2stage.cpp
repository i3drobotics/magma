/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Raffaele Solca
       @author Azzam Haidar
       @author Mark Gates

       @generated from src/zhegvdx_2stage.cpp, normal z -> d, Sat Mar 27 20:30:56 2021

*/
#include "magma_internal.h"
#include "magma_bulge.h"
#include "magma_dbulge.h"
#include "magma_timer.h"

#define REAL

/***************************************************************************//**
    Purpose
    -------
    DSYGVDX_2STAGE computes all the eigenvalues, and optionally, the eigenvectors
    of a real generalized symmetric-definite eigenproblem, of the form
    A*x=(lambda)*B*x,  A*Bx=(lambda)*x,  or B*A*x=(lambda)*x.  Here A and
    B are assumed to be symmetric and B is also positive definite.
    It uses a two-stage algorithm for the tridiagonalization.
    If eigenvectors are desired, it uses a divide and conquer algorithm.

    The divide and conquer algorithm makes very mild assumptions about
    floating point arithmetic. It will work on machines with a guard
    digit in add/subtract, or on those binary machines without guard
    digits which subtract like the Cray X-MP, Cray Y-MP, Cray C-90, or
    Cray-2. It could conceivably fail on hexadecimal or decimal machines
    without guard digits, but we know of none.

    Arguments
    ---------
    @param[in]
    itype   INTEGER
            Specifies the problem type to be solved:
      -     = 1:  A*x = (lambda)*B*x
      -     = 2:  A*B*x = (lambda)*x
      -     = 3:  B*A*x = (lambda)*x

    @param[in]
    jobz    magma_vec_t
      -     = MagmaNoVec:  Compute eigenvalues only;
      -     = MagmaVec:    Compute eigenvalues and eigenvectors.

    @param[in]
    range   magma_range_t
      -     = MagmaRangeAll: all eigenvalues will be found.
      -     = MagmaRangeV:   all eigenvalues in the half-open interval (VL,VU]
                   will be found.
      -     = MagmaRangeI:   the IL-th through IU-th eigenvalues will be found.

    @param[in]
    uplo    magma_uplo_t
      -     = MagmaUpper:  Upper triangles of A and B are stored;
      -     = MagmaLower:  Lower triangles of A and B are stored.

    @param[in]
    n       INTEGER
            The order of the matrices A and B.  N >= 0.

    @param[in,out]
    A       DOUBLE PRECISION array, dimension (LDA, N)
            On entry, the symmetric matrix A.  If UPLO = MagmaUpper, the
            leading N-by-N upper triangular part of A contains the
            upper triangular part of the matrix A.  If UPLO = MagmaLower,
            the leading N-by-N lower triangular part of A contains
            the lower triangular part of the matrix A.
    \n
            On exit, if JOBZ = MagmaVec, then if INFO = 0, A contains the
            matrix Z of eigenvectors.  The eigenvectors are normalized
            as follows:
      -     if ITYPE = 1 or 2, Z**H*B*Z = I;
      -     if ITYPE = 3, Z**H*inv(B)*Z = I.
            If JOBZ = MagmaNoVec, then on exit the upper triangle (if UPLO=MagmaUpper)
            or the lower triangle (if UPLO=MagmaLower) of A, including the
            diagonal, is destroyed.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[in,out]
    B       DOUBLE PRECISION array, dimension (LDB, N)
            On entry, the symmetric matrix B.  If UPLO = MagmaUpper, the
            leading N-by-N upper triangular part of B contains the
            upper triangular part of the matrix B.  If UPLO = MagmaLower,
            the leading N-by-N lower triangular part of B contains
            the lower triangular part of the matrix B.
    \n
            On exit, if INFO <= N, the part of B containing the matrix is
            overwritten by the triangular factor U or L from the Cholesky
            factorization B = U**H*U or B = L*L**H.

    @param[in]
    ldb     INTEGER
            The leading dimension of the array B.  LDB >= max(1,N).

    @param[in]
    vl      DOUBLE PRECISION
    @param[in]
    vu      DOUBLE PRECISION
            If RANGE=MagmaRangeV, the lower and upper bounds of the interval to
            be searched for eigenvalues. VL < VU.
            Not referenced if RANGE = MagmaRangeAll or MagmaRangeI.

    @param[in]
    il      INTEGER
    @param[in]
    iu      INTEGER
            If RANGE=MagmaRangeI, the indices (in ascending order) of the
            smallest and largest eigenvalues to be returned.
            1 <= IL <= IU <= N, if N > 0; IL = 1 and IU = 0 if N = 0.
            Not referenced if RANGE = MagmaRangeAll or MagmaRangeV.

    @param[out]
    mout    INTEGER
            The total number of eigenvalues found.  0 <= M <= N.
            If RANGE = MagmaRangeAll, M = N, and if RANGE = MagmaRangeI, M = IU-IL+1.

    @param[out]
    w       DOUBLE PRECISION array, dimension (N)
            If INFO = 0, the eigenvalues in ascending order.

    @param[out]
    work    (workspace) DOUBLE PRECISION array, dimension (MAX(1,LWORK))
            On exit, if INFO = 0, WORK[0] returns the optimal LWORK.

    @param[in]
    lwork   INTEGER
            The length of the array WORK.
     -      If N <= 1,                      LWORK >= 1.
     -      For COMPLEX ([cz]hegvdx):
       +        If JOBZ = MagmaNoVec and N > 1, LWORK >= LQ2 + N + N*NB.
       +        If JOBZ = MagmaVec   and N > 1, LWORK >= LQ2 + 2*N + N**2.
     -      For REAL ([sd]sygvdx):
       +        If JOBZ = MagmaNoVec and N > 1, LWORK >= LQ2 + 2*N + N*NB.
       +        If JOBZ = MagmaVec   and N > 1, LWORK >= LQ2 + 1 + 6*N + 2*N**2.
     -      where LQ2 is the size needed to store the Q2 matrix
            as returned by magma_bulge_get_lq2.
    \n
            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal sizes of the WORK, RWORK and
            IWORK arrays, returns these values as the first entries of
            the WORK, RWORK and IWORK arrays, and no error message
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.

*/
#ifdef COMPLEX
/**
    @param[out]
    rwork   (workspace) DOUBLE PRECISION array, dimension (MAX(1,LRWORK))
            On exit, if INFO = 0, RWORK[0] returns the optimal LRWORK.

    @param[in]
    lrwork  INTEGER
            The dimension of the array RWORK.
     -      If N <= 1,                      LRWORK >= 1.
     -      If JOBZ = MagmaNoVec and N > 1, LRWORK >= N.
     -      If JOBZ = MagmaVec   and N > 1, LRWORK >= 1 + 5*N + 2*N**2.
    \n
            If LRWORK = -1, then a workspace query is assumed; the
            routine only calculates the optimal sizes of the WORK, RWORK
            and IWORK arrays, returns these values as the first entries
            of the WORK, RWORK and IWORK arrays, and no error message
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.
*/
#endif // COMPLEX
/**

    @param[out]
    iwork   (workspace) INTEGER array, dimension (MAX(1,LIWORK))
            On exit, if INFO = 0, IWORK[0] returns the optimal LIWORK.

    @param[in]
    liwork  INTEGER
            The dimension of the array IWORK.
     -      If N <= 1,                      LIWORK >= 1.
     -      If JOBZ = MagmaNoVec and N > 1, LIWORK >= 1.
     -      If JOBZ = MagmaVec   and N > 1, LIWORK >= 3 + 5*N.
    \n
            If LIWORK = -1, then a workspace query is assumed; the
            routine only calculates the optimal sizes of the WORK, RWORK
            and IWORK arrays, returns these values as the first entries
            of the WORK, RWORK and IWORK arrays, and no error message
            related to LWORK or LRWORK or LIWORK is issued by XERBLA.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
      -     > 0:  DPOTRF or DSYEVD returned an error code:
               <= N:  if INFO = i and JOBZ = MagmaNoVec, then the algorithm
                      failed to converge; i off-diagonal elements of an
                      intermediate tridiagonal form did not converge to
                      zero;
                      if INFO = i and JOBZ = MagmaVec, then the algorithm
                      failed to compute an eigenvalue while working on
                      the submatrix lying in rows and columns INFO/(N+1)
                      through mod(INFO,N+1);
               > N:   if INFO = N + i, for 1 <= i <= N, then the leading
                      minor of order i of B is not positive definite.
                      The factorization of B could not be completed and
                      no eigenvalues or eigenvectors were computed.

    Further Details
    ---------------
    Based on contributions by
       Mark Fahey, Department of Mathematics, Univ. of Kentucky, USA

    Modified so that no backsubstitution is performed if DSYEVD fails to
    converge (NEIG in old code could be greater than N causing out of
    bounds reference to A - reported by Ralf Meyer).  Also corrected the
    description of INFO and the test on ITYPE. Sven, 16 Feb 05.

    @ingroup magma_hegvdx
*******************************************************************************/
extern "C" magma_int_t
magma_dsygvdx_2stage(
    magma_int_t itype, magma_vec_t jobz, magma_range_t range, magma_uplo_t uplo, magma_int_t n,
    double *A, magma_int_t lda,
    double *B, magma_int_t ldb,
    double vl, double vu, magma_int_t il, magma_int_t iu,
    magma_int_t *mout, double *w,
    double *work, magma_int_t lwork,
    #ifdef COMPLEX
    double *rwork, magma_int_t lrwork,
    #endif
    magma_int_t *iwork, magma_int_t liwork,
    magma_int_t *info)
{
    /* Constants */
    const double c_one = MAGMA_D_ONE;

    /* Local variables */
    magma_timer_t time=0;
    
    const char* uplo_  = lapack_uplo_const( uplo  );
    const char* jobz_  = lapack_vec_const( jobz  );

    magmaDouble_ptr dA = NULL;
    magmaDouble_ptr dB = NULL;
    magma_int_t ldda = n;
    magma_int_t lddb = n;

    magma_int_t lower;
    magma_trans_t trans;
    magma_int_t wantz;
    magma_int_t lquery;
    magma_int_t alleig, valeig, indeig;

    magma_int_t lwmin;
    magma_int_t liwmin;
    #ifdef COMPLEX
    magma_int_t lrwmin;
    #endif
    magma_queue_t queues[2] = { NULL };

    /* determine the number of threads */
    magma_int_t parallel_threads = magma_get_parallel_numthreads();

    wantz = (jobz == MagmaVec);
    lower = (uplo == MagmaLower);
    alleig = (range == MagmaRangeAll);
    valeig = (range == MagmaRangeV);
    indeig = (range == MagmaRangeI);
    lquery = (lwork == -1 || liwork == -1);
    #ifdef COMPLEX
    lquery = (lquery || lrwork == -1);
    #endif

    *info = 0;
    if (itype < 1 || itype > 3) {
        *info = -1;
    } else if (! (alleig || valeig || indeig)) {
        *info = -2;
    } else if (! (wantz || (jobz == MagmaNoVec))) {
        *info = -3;
    } else if (! (lower || (uplo == MagmaUpper))) {
        *info = -4;
    } else if (n < 0) {
        *info = -5;
    } else if (lda < max(1,n)) {
        *info = -7;
    } else if (ldb < max(1,n)) {
        *info = -9;
    } else {
        if (valeig) {
            if (n > 0 && vu <= vl) {
                *info = -11;
            }
        } else if (indeig) {
            if (il < 1 || il > max(1,n)) {
                *info = -12;
            } else if (iu < min(n,il) || iu > n) {
                *info = -13;
            }
        }
    }

    magma_int_t nb = magma_get_dbulge_nb(n, parallel_threads);
    magma_int_t lq2 = magma_get_dbulge_lq2(n, parallel_threads, wantz);

    #ifdef COMPLEX
        if (wantz) {
            lwmin  = lq2 + 2*n + n*n;
            lrwmin = 1 + 5*n + 2*n*n;
            liwmin = 5*n + 3;
        } else {
            lwmin  = lq2 + n + n*nb;
            lrwmin = n;
            liwmin = 1;
        }
    #else
        if (wantz) {
            lwmin  = lq2 + 1 + 6*n + 2*n*n;
            liwmin = 3 + 5*n;
        } else {
            lwmin  = 2*n + n*nb;
            liwmin = 1;
        }
    #endif

    work[0]  = magma_dmake_lwork( lwmin );
    #ifdef COMPLEX
    rwork[0] = magma_dmake_lwork( lrwmin );
    #endif
    iwork[0] = liwmin;

    if (lwork < lwmin && ! lquery) {
        *info = -17;
    }
    #ifdef COMPLEX
    else if (lrwork < lrwmin && ! lquery) {
        *info = -19;
    }
    #endif
    else if (liwork < liwmin && ! lquery) {
        #ifdef COMPLEX
        *info = -21;
        #else
        *info = -19;
        #endif
    }

    if (*info != 0) {
        magma_xerbla( __func__, -(*info));
        return *info;
    } else if (lquery) {
        return *info;
    }

    /* Quick return if possible */
    if (n == 0) {
        return *info;
    }

    /* If matrix is very small, then just call LAPACK on CPU, no need for GPU */
    if (n <= 128) {
        #ifdef ENABLE_DEBUG
        printf("--------------------------------------------------------------\n");
        printf("  warning matrix too small N=%lld NB=%lld, calling lapack on CPU\n", (long long) n, (long long) nb );
        printf("--------------------------------------------------------------\n");
        #endif
        lapackf77_dsygvd(&itype, jobz_, uplo_,
                         &n, A, &lda, B, &ldb,
                         w, work, &lwork,
                         #ifdef COMPLEX
                         rwork, &lrwork,
                         #endif
                         iwork, &liwork, info);
        *mout = n;
        goto cleanup;
    }

    if (MAGMA_SUCCESS != magma_dmalloc( &dA, n*ldda ) ||
        MAGMA_SUCCESS != magma_dmalloc( &dB, n*lddb )) {
        *info = MAGMA_ERR_DEVICE_ALLOC;
        goto cleanup;
    }

    magma_device_t cdev;
    magma_getdevice( &cdev );
    magma_queue_create( cdev, &queues[0] );
    magma_queue_create( cdev, &queues[1] );
    
    /* Form a Cholesky factorization of B. */
    magma_dsetmatrix( n, n, B, ldb, dB, lddb, queues[0] );
    magma_dsetmatrix_async( n, n,
                            A,  lda,
                            dA, ldda, queues[1] );

    timer_start( time );

    magma_dpotrf_gpu(uplo, n, dB, lddb, info);
    if (*info != 0) {
        *info = n + *info;
        goto cleanup;
    }

    timer_stop( time );
    timer_printf( "time dpotrf_gpu = %6.2f\n", time );

    magma_queue_sync( queues[1] );
    magma_dgetmatrix_async( n, n,
                            dB, lddb,
                            B,  ldb, queues[1] );

    /* Transform problem to standard eigenvalue problem and solve. */
    timer_start( time );
    magma_dsygst_gpu(itype, uplo, n, dA, ldda, dB, lddb, info);
    timer_stop( time );
    timer_printf( "time dsygst_gpu = %6.2f\n", time );

    magma_dgetmatrix( n, n, dA, ldda, A, lda, queues[0] );
    magma_free( dA );  dA = NULL;
    magma_free( dB );  dB = NULL;

    timer_start( time );
    magma_dsyevdx_2stage( jobz, range, uplo, n, A, lda, vl, vu, il, iu, mout, w,
                          work, lwork,
                          #ifdef COMPLEX
                          rwork, lrwork,
                          #endif
                          iwork, liwork, info );
    timer_stop( time );
    timer_printf( "time dsyevdx_2stage = %6.2f\n", time );

    if (wantz && *info == 0) {
        if (MAGMA_SUCCESS != magma_dmalloc( &dA, n*ldda ) ||
            MAGMA_SUCCESS != magma_dmalloc( &dB, n*lddb )) {
            *info = MAGMA_ERR_DEVICE_ALLOC;
            goto cleanup;
        }

        timer_start( time );

        magma_dsetmatrix( n, *mout, A, lda, dA, ldda, queues[0] );
        magma_dsetmatrix( n,  n,    B, ldb, dB, lddb, queues[0] );

        /* Backtransform eigenvectors to the original problem. */
        if (itype == 1 || itype == 2) {
            /* For A*x=(lambda)*B*x and A*B*x=(lambda)*x;
               backtransform eigenvectors: x = inv(L)'*y or inv(U)*y */
            if (lower) {
                trans = MagmaConjTrans;
            } else {
                trans = MagmaNoTrans;
            }

            magma_dtrsm( MagmaLeft, uplo, trans, MagmaNonUnit, n, *mout, c_one, dB, lddb, dA, ldda, queues[0] );
        }
        else if (itype == 3) {
            /* For B*A*x=(lambda)*x;
               backtransform eigenvectors: x = L*y or U'*y */
            if (lower) {
                trans = MagmaNoTrans;
            } else {
                trans = MagmaConjTrans;
            }

            magma_dtrmm( MagmaLeft, uplo, trans, MagmaNonUnit, n, *mout, c_one, dB, lddb, dA, ldda, queues[0] );
        }

        magma_dgetmatrix( n, *mout, dA, ldda, A, lda, queues[0] );

        timer_stop( time );
        timer_printf( "time trsm/mm + getmatrix = %6.2f\n", time );
    }

cleanup:
    magma_queue_sync( queues[0] );
    magma_queue_sync( queues[1] );
    magma_queue_destroy( queues[0] );
    magma_queue_destroy( queues[1] );
    magma_free( dA );  dA = NULL;
    magma_free( dB );  dB = NULL;

    work[0]  = magma_dmake_lwork( lwmin );
    #ifdef COMPLEX
    rwork[0] = magma_dmake_lwork( lrwmin );
    #endif
    iwork[0] = liwmin;

    return *info;
} /* magma_dsygvdx_2stage */
