#include "arithmetic.h"
#include "tools.h"
#include "sign.h"
#include "matrices.h"

int* Arithmetic::RNSadd(int* rns1, int* rns2, int* basis, int basis_size){
    int incr_basis_size = basis_size + 1;
    int* res = new int[incr_basis_size];
    for(int i = 0; i < basis_size; ++i){
        res[i] = (rns1[i] + rns2[i]) % basis[i];
    }
    res[basis_size] = sign_bit_add(rns1, rns2, basis, basis_size);
    return res;
}

int* Arithmetic::RNSmult(int* rns1, int* rns2, const int* basis, int basis_size){
    int incr_basis_size = basis_size + 1;
    int* res = new int[incr_basis_size];
    for(int i = 0; i < basis_size; ++i){
        res[i] = Tools::vec_prod(rns1, rns2, basis_size)[i] % basis[i];
    }
    res[basis_size] = sign_bit_mult(rns1, rns2, basis_size);
    return res;
}


Wrappers::RNSMatrix Arithmetic::RNS_mtx_mult(Wrappers::RNSMatrix a, Wrappers::RNSMatrix b, int* basis, int basis_size){
    Wrappers::RNSMatrix res;
    res.init();
    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 4; ++j){
            int* sum = new int[4];
            sum[0] = 0;
            sum[1] = 0;
            sum[2] = 0;
            sum[3] = 0;
            for(int k = 0; k < 4; ++k){
                int* rns1 = Tools::form_rns_vec(a, i, k, basis_size);
                int* rns2 = Tools::form_rns_vec(b, k, j, basis_size);
                sum = Arithmetic::RNSadd(sum, Arithmetic::RNSmult(rns1, rns2, basis, basis_size), basis, basis_size);
            }
            res.data[i][j][0] = sum[0];
            res.data[i][j][1] = sum[1];
            res.data[i][j][2] = sum[2];
            res.data[i][j][3] = sum[3];
        }
    }
    return res;
}
