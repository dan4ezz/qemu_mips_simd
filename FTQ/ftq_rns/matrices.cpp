#include "matrices.h"

void Wrappers::RNSMatrix::init(){
    for(int i = 0; i < 4; ++i)
        for(int j = 0; j < 4; ++j)
            for(int k = 0; k < 4; ++k)
                data[i][j][k] = 0;
}

void Wrappers::Matrix::init(int value){
    for(int i = 0; i < 4; ++i)
        for(int j = 0; j < 4; ++j)
            data[i][j] = i + value*j*i;
}

void Wrappers::Matrix::init_hadamard(){
    for(int i = 0; i < 4; ++i)
        data[0][i] = 1;
    data[1][0] = 1;
    data[1][1] = 1;
    data[1][2] = -1;
    data[1][3] = -1;
    data[2][0] = 1;
    data[2][1] = -1;
    data[2][2] = -1;
    data[2][3] = 1;
    data[3][0] = 1;
    data[3][1] = -1;
    data[3][2] = 1;
    data[3][3] = -1;
}