#pragma once

#include "matrices.h"

#include <msa.h>

namespace Convertation{
    Wrappers::Matrix RNS2dec(Wrappers::RNSMatrix, v4i32, int);
    int RNS2dec(v4i32, v4i32, int);
    Wrappers::RNSMatrix dec2RNS(Wrappers::Matrix, v4i32, int);
}