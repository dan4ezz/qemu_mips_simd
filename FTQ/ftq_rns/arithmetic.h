#pragma once

#include "matrices.h"

namespace Arithmetic{
    int* RNSadd(int*, int*, int*, int);
    int* RNSmult(int*, int*, const int*, int);
    Wrappers::RNSMatrix RNS_mtx_mult(Wrappers::RNSMatrix, Wrappers::RNSMatrix, int*, int);
}