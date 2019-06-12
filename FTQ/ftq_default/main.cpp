#include <iostream>
#include "matrix_ops.h"
#include "ftq.h"

int main() {
    FTQDefault::Matrix mtx;

    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            mtx.data[i][j] = i + i * j;
        }
    }

    FTQDefault::ftq(mtx);
}