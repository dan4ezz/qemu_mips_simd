#include "convertation.h"
#include "matrices.h"
#include "tools.h"

#include <cstdlib>
#include <msa.h>

int Convertation::RNS2dec(v4i32 rns, v4i32 basis, int basis_size){
    int M = Tools::prod(basis, basis_size);
    v4i32 Mi, bi;
    for(int i = 0; i < basis_size; ++i){
        Mi[i] = M/basis[i];
        int coef = Tools::gcd_coef(Mi[i], basis[i]);
        while(coef < 0){
            coef += basis[i];
        }
        bi[i] = coef;
    }

    int dec;

    if (rns[basis_size] == 1){
        dec = Tools::vec_sum(Tools::vec_prod(Tools::vec_prod(rns, Mi, basis_size), bi, basis_size), basis_size) % M - M;
    }
    else{
        dec = Tools::vec_sum(Tools::vec_prod(Tools::vec_prod(rns, Mi, basis_size), bi, basis_size), basis_size) % M;
    }

    return dec;
}

Wrappers::Matrix Convertation::RNS2dec(Wrappers::RNSMatrix rns, v4i32 basis, int basis_size){
    int M = Tools::prod(basis, basis_size);
    v4i32 Mi, bi;

    for(int i = 0; i < basis_size; ++i){
        Mi[i] = M/basis[i];
        int coef = Tools::gcd_coef(Mi[i], basis[i]);
        while(coef < 0){
            coef += basis[i];
        }
        bi[i] = coef;
    }

    Wrappers::Matrix dec;

    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 4; ++j){
            if (rns.data[i][j][basis_size] == 1) {
                dec.data[i][j] = Tools::vec_sum(Tools::vec_prod(rns.data[i][j]*Mi, bi, basis_size), basis_size) % M - M;
            }
            else {
                dec.data[i][j] = Tools::vec_sum(Tools::vec_prod(rns.data[i][j] * Mi, bi, basis_size), basis_size) % M;
            }
        }
    }

    return dec;
}

Wrappers::RNSMatrix Convertation::dec2RNS(Wrappers::Matrix dec, v4i32 basis, int basis_size){
    int M = Tools::prod(basis, basis_size);

    Wrappers::RNSMatrix res;
    res.init();

    for(int i = 0; i < 4; ++i){
        for(int j = 0; j < 4; ++j){

                if (dec.data[i][j] < 0) {
                    res.data[i][j] = (dec.data[i][j] + M) % basis;
                    res.data[i][j][basis_size] = 1;
                }

                else {
                    res.data[i][j] = dec.data[i][j] % basis;
                    res.data[i][j][basis_size] = 0;
                }
            
        }
    }

    return res;
}