/*
    -- MAGMA (version 2.0) --
       Univ. of Tennessee, Knoxville
       Univ. of California, Berkeley
       Univ. of Colorado, Denver
       @date

       @author Raffaele Solca
       @author Azzam Haidar

       @generated from src/zheevx.cpp, normal z -> c, Sat Mar 27 20:30:51 2021

 */
#include "magma_internal.h"

/***************************************************************************//**
    Purpose
    -------
    CHEEVX computes selected eigenvalues and, optionally, eigenvectors
    of a complex Hermitian matrix A.  Eigenvalues and eigenvectors can
    be selected by specifying either a range of values or a range of
    indices for the desired eigenvalues.

    Arguments
    ---------
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
      -     = MagmaUpper:  Upper triangle of A is stored;
      -     = MagmaLower:  Lower triangle of A is stored.

    @param[in]
    n       INTEGER
            The order of the matrix A.  N >= 0.

    @param[in,out]
    A       COMPLEX array, dimension (LDA, N)
            On entry, the Hermitian matrix A.  If UPLO = MagmaUpper, the
            leading N-by-N upper triangular part of A contains the
            upper triangular part of the matrix A.  If UPLO = MagmaLower,
            the leading N-by-N lower triangular part of A contains
            the lower triangular part of the matrix A.
            On exit, the lower triangle (if UPLO=MagmaLower) or the upper
            triangle (if UPLO=MagmaUpper) of A, including the diagonal, is
            destroyed.

    @param[in]
    lda     INTEGER
            The leading dimension of the array A.  LDA >= max(1,N).

    @param[in]
    vl      REAL
    @param[in]
    vu      REAL
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

    @param[in]
    abstol  REAL
            The absolute error tolerance for the eigenvalues.
            An approximate eigenvalue is accepted as converged
            when it is determined to lie in an interval [a,b]
            of width less than or equal to

                    ABSTOL + EPS * max( |a|,|b| ),
    \n
            where EPS is the machine precision.  If ABSTOL is less than
            or equal to zero, then  EPS*|T|  will be used in its place,
            where |T| is the 1-norm of the tridiagonal matrix obtained
            by reducing A to tridiagonal form.
    \n
            Eigenvalues will be computed most accurately when ABSTOL is
            set to twice the underflow threshold 2*SLAMCH('S'), not zero.
            If this routine returns with INFO > 0, indicating that some
            eigenvectors did not converge, try setting ABSTOL to
            2*SLAMCH('S').
    \n
            See "Computing Small Singular Values of Bidiagonal Matrices
            with Guaranteed High Relative Accuracy," by Demmel and
            Kahan, LAPACK Working Note #3.

    @param[out]
    m       INTEGER
            The total number of eigenvalues found.  0 <= M <= N.
            If RANGE = MagmaRangeAll, M = N, and if RANGE = MagmaRangeI, M = IU-IL+1.

    @param[out]
    w       REAL array, dimension (N)
            On normal exit, the first M elements contain the selected
            eigenvalues in ascending order.

    @param[out]
    Z       COMPLEX array, dimension (LDZ, max(1,M))
            If JOBZ = MagmaVec, then if INFO = 0, the first M columns of Z
            contain the orthonormal eigenvectors of the matrix A
            corresponding to the selected eigenvalues, with the i-th
            column of Z holding the eigenvector associated with W(i).
            If an eigenvector fails to converge, then that column of Z
            contains the latest approximation to the eigenvector, and the
            index of the eigenvector is returned in IFAIL.
            If JOBZ = MagmaNoVec, then Z is not referenced.
            Note: the user must ensure that at least max(1,M) columns are
            supplied in the array Z; if RANGE = MagmaRangeV, the exact value of M
            is not known in advance and an upper bound must be used.

    @param[in]
    ldz     INTEGER
            The leading dimension of the array Z.  LDZ >= 1, and if
            JOBZ = MagmaVec, LDZ >= max(1,N).

    @param[out]
    work    (workspace) COMPLEX array, dimension (LWORK)
            On exit, if INFO = 0, WORK[0] returns the optimal LWORK.

    @param[in]
    lwork   INTEGER
            The length of the array WORK.  LWORK >= max(1,2*N-1).
            For optimal efficiency, LWORK >= (NB+1)*N,
            where NB is the max of the blocksize for CHETRD and for
            CUNMTR as returned by ILAENV.
    \n
            If LWORK = -1, then a workspace query is assumed; the routine
            only calculates the optimal size of the WORK array, returns
            this value as the first entry of the WORK array, and no error
            message related to LWORK is issued by XERBLA.

    @param
    rwork   (workspace) REAL array, dimension (7*N)

    @param
    iwork   (workspace) INTEGER array, dimension (5*N)

    @param[out]
    ifail   INTEGER array, dimension (N)
            If JOBZ = MagmaVec, then if INFO = 0, the first M elements of
            IFAIL are zero.  If INFO > 0, then IFAIL contains the
            indices of the eigenvectors that failed to converge.
            If JOBZ = MagmaNoVec, then IFAIL is not referenced.

    @param[out]
    info    INTEGER
      -     = 0:  successful exit
      -     < 0:  if INFO = -i, the i-th argument had an illegal value
      -     > 0:  if INFO = i, then i eigenvectors failed to converge.
                  Their indices are stored in array IFAIL.

    @ingroup magma_heevx
*******************************************************************************/
extern "C" magma_int_t
magma_cheevx(
    magma_vec_t jobz, magma_range_t range, magma_uplo_t uplo, magma_int_t n,
    magmaFloatComplex *A, magma_int_t lda,
    float vl, float vu,
    magma_int_t il, magma_int_t iu,
    float abstol,
    magma_int_t *m, float *w,
    magmaFloatComplex *Z, magma_int_t ldz,
    magmaFloatComplex *work, magma_int_t lwork,
    float *rwork,
    magma_int_t *iwork, magma_int_t *ifail,
    magma_int_t *info)
{
    /* Constants */
    const magma_int_t izero = 0;
    const magma_int_t ione  = 1;
    
    /* Local variables */
    const char* uplo_  = lapack_uplo_const( uplo  );
    const char* jobz_  = lapack_vec_const( jobz  );
    const char* range_ = lapack_range_const( range );
    
    const char* order_;
    magma_int_t indd, inde;
    magma_int_t imax;
    magma_int_t lopt, itmp1, indee;
    magma_int_t i, j, jj, i__1;
    magma_int_t iscale, indibl;
    magma_int_t indiwk, indisp, indtau;
    magma_int_t indrwk, indwrk;
    magma_int_t llwork, nsplit;
    magma_int_t iinfo;
    float safmin;
    float bignum;
    float smlnum;
    float eps, tmp1;
    float anrm;
    float sigma, d__1;
    float rmin, rmax;
    
    /* Function Body */
    bool lower  = (uplo  == MagmaLower);
    bool wantz  = (jobz  == MagmaVec);
    bool alleig = (range == MagmaRangeAll);
    bool valeig = (range == MagmaRangeV);
    bool indeig = (range == MagmaRangeI);
    bool lquery = (lwork == -1);
    
    *info = 0;
    if (! (wantz || (jobz == MagmaNoVec))) {
        *info = -1;
    } else if (! (alleig || valeig || indeig)) {
        *info = -2;
    } else if (! (lower || (uplo == MagmaUpper))) {
        *info = -3;
    } else if (n < 0) {
        *info = -4;
    } else if (lda < max(1,n)) {
        *info = -6;
    } else if (ldz < 1 || (wantz && ldz < n)) {
        *info = -15;
    } else {
        if (valeig) {
            if (n > 0 && vu <= vl) {
                *info = -8;
            }
        } else if (indeig) {
            if (il < 1 || il > max(1,n)) {
                *info = -9;
            } else if (iu < min(n,il) || iu > n) {
                *info = -10;
            }
        }
    }
    
    magma_int_t nb = magma_get_chetrd_nb(n);
    
    lopt = n * (nb + 1);
    
    work[0] = magma_cmake_lwork( lopt );
    
    if (lwork < lopt && ! lquery) {
        *info = -17;
    }
    
    if (*info != 0) {
        magma_xerbla( __func__, -(*info));
        return *info;
    } else if (lquery) {
        return *info;
    }
    
    *m = 0;
    /* Check if matrix is very small then just call LAPACK on CPU, no need for GPU */
    if (n <= 128) {
        #ifdef ENABLE_DEBUG
        printf("--------------------------------------------------------------\n");
        printf("  warning matrix too small N=%lld NB=%lld, calling lapack on CPU\n", (long long) n, (long long) nb );
        printf("--------------------------------------------------------------\n");
        #endif
        lapackf77_cheevx(jobz_, range_, uplo_,
                         &n, A, &lda, &vl, &vu, &il, &iu, &abstol, m,
                         w, Z, &ldz, work, &lwork,
                         rwork, iwork, ifail, info);
        return *info;
    }
    
    --w;
    --work;
    --rwork;
    --iwork;
    --ifail;
    
    /* Get machine constants. */
    safmin = lapackf77_slamch("Safe minimum");
    eps = lapackf77_slamch("Precision");
    smlnum = safmin / eps;
    bignum = 1. / smlnum;
    rmin = magma_ssqrt(smlnum);
    rmax = magma_ssqrt(bignum);
    
    /* Scale matrix to allowable range, if necessary. */
    anrm = lapackf77_clanhe("M", uplo_, &n, A, &lda, &rwork[1]);
    iscale = 0;
    if (anrm > 0. && anrm < rmin) {
        iscale = 1;
        sigma = rmin / anrm;
    } else if (anrm > rmax) {
        iscale = 1;
        sigma = rmax / anrm;
    }
    if (iscale == 1) {
        d__1 = 1.;
        lapackf77_clascl(uplo_, &izero, &izero, &d__1, &sigma, &n, &n, A,
                         &lda, info);
        
        if (abstol > 0.) {
            abstol *= sigma;
        }
        if (valeig) {
            vl *= sigma;
            vu *= sigma;
        }
    }
    
    /* Call CHETRD to reduce Hermitian matrix to tridiagonal form. */
    indd = 1;
    inde = indd + n;
    indrwk = inde + n;
    indtau = 1;
    indwrk = indtau + n;
    llwork = lwork - indwrk + 1;
    
    magma_chetrd(uplo, n, A, lda, &rwork[indd], &rwork[inde], &work[indtau], &work[indwrk], llwork, &iinfo);
    
    lopt = n + (magma_int_t)MAGMA_C_REAL(work[indwrk]);
    
    /* If all eigenvalues are desired and ABSTOL is less than or equal to
       zero, then call SSTERF or CUNGTR and CSTEQR.  If this fails for
       some eigenvalue, then try SSTEBZ. */
    if ((alleig || (indeig && il == 1 && iu == n)) && abstol <= 0.) {
        blasf77_scopy(&n, &rwork[indd], &ione, &w[1], &ione);
        indee = indrwk + 2*n;
        if (! wantz) {
            i__1 = n - 1;
            blasf77_scopy(&i__1, &rwork[inde], &ione, &rwork[indee], &ione);
            lapackf77_ssterf(&n, &w[1], &rwork[indee], info);
        }
        else {
            lapackf77_clacpy("A", &n, &n, A, &lda, Z, &ldz);
            lapackf77_cungtr(uplo_, &n, Z, &ldz, &work[indtau], &work[indwrk], &llwork, &iinfo);
            i__1 = n - 1;
            blasf77_scopy(&i__1, &rwork[inde], &ione, &rwork[indee], &ione);
            lapackf77_csteqr(jobz_, &n, &w[1], &rwork[indee], Z, &ldz, &rwork[indrwk], info);
            if (*info == 0) {
                for (i = 1; i <= n; ++i) {
                    ifail[i] = 0;
                }
            }
        }
        if (*info == 0) {
            *m = n;
        }
    }
    
    /* Otherwise, call SSTEBZ and, if eigenvectors are desired, CSTEIN. */
    if (*m == 0) {
        *info = 0;
        if (wantz) {
            order_ = "B";
        } else {
            order_ = "E";
        }
        indibl = 1;
        indisp = indibl + n;
        indiwk = indisp + n;
        lapackf77_sstebz(range_, order_, &n, &vl, &vu, &il, &iu, &abstol, &rwork[indd], &rwork[inde], m,
                         &nsplit, &w[1], &iwork[indibl], &iwork[indisp], &rwork[indrwk], &iwork[indiwk], info);
        
        if (wantz) {
            lapackf77_cstein(&n, &rwork[indd], &rwork[inde], m, &w[1], &iwork[indibl], &iwork[indisp],
                             Z, &ldz, &rwork[indrwk], &iwork[indiwk], &ifail[1], info);
            
            /* Apply unitary matrix used in reduction to tridiagonal
               form to eigenvectors returned by CSTEIN. */
            magma_cunmtr(MagmaLeft, uplo, MagmaNoTrans, n, *m, A, lda, &work[indtau],
                         Z, ldz, &work[indwrk], llwork, &iinfo);
        }
    }
    /* If matrix was scaled, then rescale eigenvalues appropriately. */
    if (iscale == 1) {
        if (*info == 0) {
            imax = *m;
        } else {
            imax = *info - 1;
        }
        d__1 = 1. / sigma;
        blasf77_sscal(&imax, &d__1, &w[1], &ione);
    }
    
    /* If eigenvalues are not in order, then sort them, along with
       eigenvectors. */
    if (wantz) {
        for (j = 1; j <= *m-1; ++j) {
            i = 0;
            tmp1 = w[j];
            for (jj = j + 1; jj <= *m; ++jj) {
                if (w[jj] < tmp1) {
                    i = jj;
                    tmp1 = w[jj];
                }
            }
            
            if (i != 0) {
                itmp1 = iwork[indibl + i - 1];
                w[i] = w[j];
                iwork[indibl + i - 1] = iwork[indibl + j - 1];
                w[j] = tmp1;
                iwork[indibl + j - 1] = itmp1;
                blasf77_cswap(&n, Z + (i-1)*ldz, &ione, Z + (j-1)*ldz, &ione);
                if (*info != 0) {
                    itmp1 = ifail[i];
                    ifail[i] = ifail[j];
                    ifail[j] = itmp1;
                }
            }
        }
    }
    
    /* Set WORK[0] to optimal complex workspace size. */
    work[1] = magma_cmake_lwork( lopt );
    
    return *info;
} /* magma_cheevx */
