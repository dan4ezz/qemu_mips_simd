#include "convertation.h"
#include "matrices.h"
#include "tools.h"

#include <cstdlib>
//
//int* Convertation::vec_prod(Wrappers::RNSMatrix a, const int* b, int col, int row, int basis_size){
//    int* res = new int[basis_size];
//    for(int i = 0; i < basis_size; ++i){
//        res[i] = a.data[col][row][i]*b[i];
//    }
//    return res;
//}
//
//int Convertation::RNS2dec(int* rns, int* basis, int basis_size){
//    int M = Tools::prod(basis, basis_size);
//    int* Mi = new int[basis_size];
//    int* bi = new int[basis_size];
//    for(int i = 0; i < basis_size; ++i){
//        Mi[i] = M/basis[i];
//        int coef = Tools::gcd_coef(Mi[i], basis[i]);
//        while(coef < 0){
//            coef += basis[i];
//        }
//        bi[i] = coef;
//    }
//
//    int dec;
//
//    if (rns[basis_size] == 1){
//        dec = Tools::vec_sum(Tools::vec_prod(Tools::vec_prod(rns, Mi, basis_size), bi, basis_size), basis_size) % M - M;
//    }
//    else{
//        dec = Tools::vec_sum(Tools::vec_prod(Tools::vec_prod(rns, Mi, basis_size), bi, basis_size), basis_size) % M;
//    }
//
//    return dec;
//}
//
//Wrappers::Matrix Convertation::RNS2dec(Wrappers::RNSMatrix rns, int* basis, int basis_size){
//    int M = Tools::prod(basis, basis_size);
//    int* Mi = new int[basis_size];
//    int* bi = new int[basis_size];
//
//    for(int i = 0; i < basis_size; ++i){
//        Mi[i] = M/basis[i];
//        int coef = Tools::gcd_coef(Mi[i], basis[i]);
//        while(coef < 0){
//            coef += basis[i];
//        }
//        bi[i] = coef;
//    }
//
//    Wrappers::Matrix dec;
//
//    for(int i = 0; i < 4; ++i){
//        for(int j = 0; j < 4; ++j){
//            if (rns.data[i][j][basis_size] == 1) {
//                dec.data[i][j] = Tools::vec_sum(Tools::vec_prod(Convertation::vec_prod(rns, Mi, i, j, basis_size), bi, basis_size), basis_size) % M - M;
//            }
//            else {
//                dec.data[i][j] = Tools::vec_sum(Tools::vec_prod(Convertation::vec_prod(rns, Mi, i, j, basis_size), bi, basis_size), basis_size) % M;
//            }
//        }
//    }
//
//    return dec;
//}
//
//Wrappers::RNSMatrix Convertation::dec2RNS(Wrappers::Matrix dec, int* basis, int basis_size){
//    int M = Tools::prod(basis, basis_size);
//
//    Wrappers::RNSMatrix res;
//    res.init();
//
//    for(int i = 0; i < 4; ++i){
//        for(int j = 0; j < 4; ++j){
//            for(int k = 0; k < basis_size; ++k){
//
//                if (dec.data[i][j] < 0) {
//                    res.data[i][j][k] = (dec.data[i][j] + M) % basis[k];
//                    res.data[i][j][basis_size] = 1;
//                }
//
//                else {
//                    res.data[i][j][k] = dec.data[i][j] % basis[k];
//                    res.data[i][j][basis_size] = 0;
//                }
//            }
//        }
//    }
//
//    return res;
//}
