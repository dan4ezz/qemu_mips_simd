#pragma once

#include "matrices.h"

#include <msa.h>

namespace Arithmetic{
    v4i32 RNSadd(v4i32, v4i32, v4i32, int);
    v4i32 RNSmult(v4i32, v4i32, v4i32, int);
    Wrappers::RNSMatrix RNS_mtx_mult(Wrappers::RNSMatrix, Wrappers::RNSMatrix, v4i32, int);
}