/*
   -- MAGMA (version 2.0) --
   Univ. of Tennessee, Knoxville
   Univ. of California, Berkeley
   Univ. of Colorado, Denver
   @date

   @author Azzam Haidar
   @author Adrien Remy

   @generated from src/zgetrf_panel_nopiv_batched.cpp, normal z -> s, Sat Mar 27 20:31:08 2021
*/

#include "magma_internal.h"

/******************************************************************************/
extern "C" magma_int_t
magma_sgetrf_panel_nopiv_batched(
    magma_int_t m, magma_int_t nb,    
    float** dA_array,    magma_int_t ldda,
    float** dX_array,    magma_int_t dX_length,
    float** dinvA_array, magma_int_t dinvA_length,
    float** dW0_displ, float** dW1_displ,  
    float** dW2_displ, float** dW3_displ,
    float** dW4_displ,     
    magma_int_t *info_array, magma_int_t gbstep,  
    magma_int_t batchCount, magma_queue_t queue )
{
    magma_int_t arginfo = 0;
    //===============================================
    //  panel factorization
    //===============================================
    if (m < nb) {
        printf("magma_sgetrf_panel_nopiv_batched_q m < nb %lld < %lld\n", (long long) m, (long long) nb );
        return -101;
    }

#if 0
    arginfo = magma_sgetf2_nopiv_batched(
                       m, nb,
                       dA_array, ldda,
                       dW1_displ, dW2_displ, dW3_displ,
                       info_array, gbstep, batchCount);
    if (arginfo != 0) return arginfo;
#else
    arginfo = magma_sgetf2_nopiv_batched(
                       nb, nb,
                       dA_array, 0, 0, ldda,
                       info_array, gbstep, batchCount, queue);
    if (arginfo != 0) return arginfo;
    if ((m-nb) > 0) {
        magma_sdisplace_pointers(dW0_displ, dA_array, ldda, nb, 0, batchCount, queue);
        magmablas_strsm_inv_work_batched( MagmaRight, MagmaUpper, MagmaNoTrans, MagmaNonUnit,
                              1, m-nb, nb, 
                              MAGMA_S_ONE,
                              dA_array,    ldda, 
                              dW0_displ,   ldda, 
                              dX_array,    m-nb, 
                              dinvA_array, dinvA_length, 
                              dW1_displ,   dW2_displ, 
                              dW3_displ,   dW4_displ,
                              1, batchCount, queue );
    }
#endif
    return 0;
}


/******************************************************************************/
extern "C" magma_int_t
magma_sgetrf_recpanel_nopiv_batched(
    magma_int_t m, magma_int_t n, magma_int_t min_recpnb,    
    float** dA_array,    magma_int_t ldda,
    float** dX_array,    magma_int_t dX_length,
    float** dinvA_array, magma_int_t dinvA_length,
    float** dW1_displ, float** dW2_displ,  
    float** dW3_displ, float** dW4_displ,
    float** dW5_displ, 
    magma_int_t *info_array, magma_int_t gbstep,
    magma_int_t batchCount, magma_queue_t queue )
{
    // Quick return if possible
    if (m == 0 || n == 0) {
        return 0;
    }
    magma_int_t arginfo = 0;


    float **dA_displ  = NULL;
    magma_malloc((void**)&dA_displ,   batchCount * sizeof(*dA_displ));
    
    magma_int_t panel_nb = n;
    if (panel_nb <= min_recpnb) {
        // if (DEBUG > 0)printf("calling bottom panel recursive with m=%d nb=%d\n",m,n);
        //  panel factorization
        //magma_sdisplace_pointers(dA_displ, dA_array, ldda, 0, 0, batchCount);
        arginfo = magma_sgetrf_panel_nopiv_batched(
                           m, panel_nb, 
                           dA_array, ldda,
                           dX_array, dX_length,
                           dinvA_array, dinvA_length,
                           dW1_displ, dW2_displ,
                           dW3_displ, dW4_displ, dW5_displ,
                           info_array, gbstep, batchCount, queue);
        if (arginfo != 0) return arginfo;
    }
    else {
        // split A over two [A A2]
        // panel on A1, update on A2 then panel on A1    
        magma_int_t n1 = n/2;
        magma_int_t n2 = n-n1;
        magma_int_t m1 = m;
        magma_int_t m2 = m-n1;
        magma_int_t p1 = 0;
        magma_int_t p2 = n1;
        // panel on A1
        //printf("calling recursive panel on A1 with m=%d nb=%d min_recpnb %d\n",m1,n1,min_recpnb);
        magma_sdisplace_pointers(dA_displ, dA_array, ldda, p1, p1, batchCount, queue); 
        arginfo = magma_sgetrf_recpanel_nopiv_batched(
                           m1, n1, min_recpnb,
                           dA_displ, ldda,
                           dX_array, dX_length,
                           dinvA_array, dinvA_length,
                           dW1_displ, dW2_displ,
                           dW3_displ, dW4_displ, dW5_displ,
                           info_array, gbstep, batchCount, queue);
        if (arginfo != 0) return arginfo;

        // update A2
        //printf("calling update A2 with             m=%d n=%d k=%d\n",m2,n2,n1);
        
        magma_sdisplace_pointers(dW5_displ, dA_array, ldda, p1, p2, batchCount, queue); 
        magmablas_strsm_inv_work_batched( MagmaLeft, MagmaLower, MagmaNoTrans, MagmaUnit, 1,
                              n1, n2,
                              MAGMA_S_ONE,
                              dA_displ,    ldda, // dA
                              dW5_displ,   ldda, // dB
                              dX_array,  n1, // dX
                              dinvA_array, dinvA_length,
                              dW1_displ,   dW2_displ, 
                              dW3_displ,   dW4_displ,
                              1, batchCount, queue );

        magma_sdisplace_pointers(dW1_displ, dA_array, ldda, p2, 0, batchCount, queue); 
        magma_sdisplace_pointers(dA_displ, dA_array, ldda, p2, p2, batchCount, queue); 
        magma_sgemm_batched( MagmaNoTrans, MagmaNoTrans, m2, n2, n1, 
                             MAGMA_S_NEG_ONE, dW1_displ, ldda, 
                             dW5_displ, ldda, 
                             MAGMA_S_ONE,  dA_displ, ldda, 
                             batchCount, queue );
        // panel on A2
        //printf("calling recursive panel on A2 with m=%d nb=%d min_recpnb %d\n",m2,n2,min_recpnb);
        arginfo = magma_sgetrf_recpanel_nopiv_batched(
                           m2, n2, min_recpnb,
                           dA_displ, ldda,
                           dX_array, dX_length,
                           dinvA_array, dinvA_length,
                           dW1_displ, dW2_displ,
                           dW3_displ, dW4_displ, dW5_displ,
                           info_array, gbstep+p2, batchCount, queue);
        if (arginfo != 0) return arginfo;
    }

    magma_free(dA_displ);
    return 0;
}
