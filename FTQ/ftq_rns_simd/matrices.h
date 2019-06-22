#pragma once
#include <msa.h>

namespace Wrappers{

    struct Matrix{
        v4i32 data[4];
        void init(int value = 5);
        void init_hadamard();
    };

    struct RNSMatrix{
        v4i32 data[4][4];
        void init();
    };

}