#include "matrix_ops.h"

using FTQDefault::Matrix;

Matrix FTQDefault::MatrixOperations::multiply(Matrix mtx1, Matrix mtx2) {
    Matrix mtx{};

    for(int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            mtx.data[i][j] = 0;
            for (int k = 0; k < 4; ++k) {
                mtx.data[i][j] += mtx1.data[i][k] * mtx2.data[k][j];
            }
        }
    }

    return mtx;
}

Matrix FTQDefault::MatrixOperations::transpose(Matrix mtx) {
    Matrix res;

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            res.data[j][i] = mtx.data[i][j];
        }
    }

    return res;
}