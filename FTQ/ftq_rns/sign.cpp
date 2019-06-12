#include "sign.h"
#include "convertation.h"
#include "tools.h"

#include <cstdlib>

//bool sign_bit_add(int* rns1, int* rns2, int* basis, int basis_size){
//    if (rns1[basis_size] == 0 && rns2[basis_size] == 0)
//        return false; // Åñëè îáà àðãóìåíòà >=0, òî ðåçóëüòàò >=0.
//    else if (rns1[basis_size] == 1 && rns2[basis_size] == 1)
//        return true;  // Åñëè îáà àðãóìåíòà <0, òî ðåçóëüòàò <0.
//    else if (rns1[basis_size] == 1 && rns2[basis_size] == 0){
//        int dec1 = Convertation::RNS2dec(rns1, basis, basis_size);
//        int dec2 = Convertation::RNS2dec(rns2, basis, basis_size);
//        return abs(dec1) > abs(dec2);
//    }
//    else if (rns1[basis_size] == 0 && rns2[basis_size] == 1){
//        int dec1 = Convertation::RNS2dec(rns1, basis, basis_size);
//        int dec2 = Convertation::RNS2dec(rns2, basis, basis_size);
//        return abs(dec1) < abs(dec2);
//    }
//}
//
//bool sign_bit_mult(int* rns1, int* rns2, int basis_size){
//    if(rns1[basis_size] == 0 && rns2[basis_size] == 0)
//        return false;
//    else if (rns1[basis_size] == 1 && rns2[basis_size] == 1)
//        return false;
//    else
//        return !(Tools::vec_sum(rns1, basis_size) == 0 || Tools::vec_sum(rns2, basis_size) == 0);
//}
