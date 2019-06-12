#include "matrix_ops.h"
#include "ftq.h"

void FTQDefault::ftq(FTQDefault::Matrix mtx) {
    Matrix C;
    //????????
    C.data[0][0] = 1; C.data[0][1] = 1; C.data[0][2] = 1; C.data[0][3] = 1;
    C.data[1][0] = 2; C.data[1][1] = -1; C.data[1][2] = -1; C.data[1][3] = -2;
    C.data[2][0] = 1; C.data[2][1] = -1; C.data[2][2] = -1; C.data[1][3] = 1;
    C.data[3][0] = 1; C.data[3][1] = -2; C.data[3][2] = 2; C.data[1][3] = -1;

    Matrix CT = MatrixOperations::transpose(C);

    Matrix W = MatrixOperations::multiply(MatrixOperations::multiply(C, mtx), CT);

    Matrix H;

    H.data[0][0] = 0.5; H.data[0][1] = 0.5; H.data[0][2] = 0.5; H.data[0][3] = 0.5;
    H.data[1][0] = 0.5; H.data[1][1] = 0.5; H.data[1][2] = -0.5; H.data[1][3] = -0.5;
    H.data[2][0] = 0.5; H.data[2][1] = -0.5; H.data[2][2] = -0.5; H.data[2][3] = 0.5;
    H.data[3][0] = 0.5; H.data[3][1] = -0.5; H.data[3][2] = 0.5; H.data[3][3] = -0.5;

    Matrix HT = MatrixOperations::transpose(H);

    Matrix Y = MatrixOperations::multiply(MatrixOperations::multiply(H, W), HT);
}

