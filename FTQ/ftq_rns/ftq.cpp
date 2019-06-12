#include "ftq.h"
#include "matrices.h"
#include "arithmetic.h"
#include "convertation.h"
#include "tools.h"

#include <iostream>
//using std::cout;
//
//void FTQ_RNS::FTQ_RNS(Wrappers::Matrix X, int* basis, int basis_size){
//    using Wrappers::RNSMatrix;
//    using Wrappers::Matrix;
//    using Arithmetic::RNS_mtx_mult;
//    using Convertation::dec2RNS;
//    using Convertation::RNS2dec;
//
//    RNSMatrix X_RNS = dec2RNS(X, basis, basis_size);
//
//    Matrix C, CT, H, HT;
//
//    C.init(1);
//    CT = Tools::transpose_mtx(C);
//
//    H.init_hadamard();
//    HT = Tools::transpose_mtx(H);
//
//    RNSMatrix C_RNS = dec2RNS(C, basis, basis_size);
//    RNSMatrix CT_RNS = dec2RNS(CT, basis, basis_size);
//
//    RNSMatrix H_RNS = dec2RNS(H, basis, basis_size);
//    RNSMatrix HT_RNS = dec2RNS(HT, basis, basis_size);
//
//    RNSMatrix W_RNS = RNS_mtx_mult(RNS_mtx_mult(C_RNS, X_RNS, basis, basis_size), CT_RNS, basis, basis_size); // DCT-transform
//    RNSMatrix Y_RNS = RNS_mtx_mult(RNS_mtx_mult(H_RNS, W_RNS, basis, basis_size), HT_RNS, basis, basis_size); // Hadamard-transform
//
//    Matrix Y = RNS2dec(Y_RNS, basis, basis_size);
//}