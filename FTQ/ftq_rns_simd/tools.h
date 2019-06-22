#pragma once

#include "matrices.h"
#include <msa.h>

namespace Tools{
    int prod(v4i32, int);
    v4i32 vec_prod(v4i32, v4i32, int);
    int vec_sum(v4i32, int);
    int gcd_coef(long long, long long);
    void print_RNSMatrix(Wrappers::RNSMatrix);
    void print_Matrix(Wrappers::Matrix);
    Wrappers::Matrix transpose_mtx(Wrappers::Matrix);
}