#include "arithmetic.h"
#include "tools.h"
#include "sign.h"
#include "matrices.h"

#include <msa.h>

v4i32 Arithmetic::RNSadd(v4i32 rns1, v4i32 rns2, v4i32 basis, int basis_size){
    v4i32 res = (rns1 + rns2) % basis;
    res[basis_size] = sign_bit_add(rns1, rns2, basis, basis_size);
    return res;
}

v4i32 Arithmetic::RNSmult(v4i32 rns1, v4i32 rns2, v4i32 basis, int basis_size){
    v4i32 res = Tools::vec_prod(rns1, rns2, basis_size) % basis;
    res[basis_size] = sign_bit_mult(rns1, rns2, basis_size);
    return res;
}


Wrappers::RNSMatrix Arithmetic::RNS_mtx_mult(Wrappers::RNSMatrix a, Wrappers::RNSMatrix b, v4i32 basis, int basis_size){
    Wrappers::RNSMatrix res;
    res.init();
    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 4; ++j){
            v4i32 sum = __builtin_msa_fill_w(0);

            for(int k = 0; k < 4; ++k){
                v4i32 rns1 = a.data[i][k];
                v4i32 rns2 = b.data[k][j];
                sum = Arithmetic::RNSadd(sum, Arithmetic::RNSmult(rns1, rns2, basis, basis_size), basis, basis_size);
            }
            res.data[i][j] = sum;
        }
    }
    return res;
}